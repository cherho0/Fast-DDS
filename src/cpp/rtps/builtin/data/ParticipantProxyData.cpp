// Copyright 2016 Proyectos y Sistemas de Mantenimiento SL (eProsima).
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/**
 * @file ParticipantProxyData.cpp
 *
 */

#include <fastrtps/rtps/builtin/data/ParticipantProxyData.h>
#include <fastrtps/rtps/builtin/discovery/participant/PDP.h>
#include <fastrtps/rtps/builtin/data/WriterProxyData.h>
#include <fastrtps/rtps/builtin/data/ReaderProxyData.h>
#include <fastrtps/rtps/builtin/discovery/participant/PDPSimple.h>
#include <fastrtps/rtps/resources/TimedEvent.h>
#include <fastrtps/rtps/builtin/BuiltinProtocols.h>
#include <rtps/participant/RTPSParticipantImpl.h>
#include <fastrtps/log/Log.h>
#include <fastrtps/qos/QosPolicies.h>
#include <fastrtps/utils/TimeConversion.h>

#include <mutex>
#include <chrono>

using namespace eprosima::fastrtps;


namespace eprosima {
namespace fastrtps{
namespace rtps {

/**
    * ParticipantProxyData objects are now shared among several participants PDP and
    * are managed from a static pool in PDP thus liveliness is not each participant responsability.
    * Each participant PDP should use shared pointers to enable the framework to track when
    * a proxy can be returned to the pool. When no proxy is referenced in any PDP it returns to the pool.
*/

void ParticipantProxyData::pool_deleter::operator()(
    ParticipantProxyData* p) const
{
    PDP::return_participant_proxy_to_pool(p);
}

ParticipantProxyData::ParticipantProxyData(const RTPSParticipantAllocationAttributes& allocation)
    : m_protocolVersion(c_ProtocolVersion)
    , m_VendorId(c_VendorId_Unknown)
    , m_expectsInlineQos(false)
    , m_availableBuiltinEndpoints(0)
    , metatraffic_locators(allocation.locators.max_unicast_locators, allocation.locators.max_multicast_locators)
    , default_locators(allocation.locators.max_unicast_locators, allocation.locators.max_multicast_locators)
#if HAVE_SECURITY
    , security_attributes_(0UL)
    , plugin_security_attributes_(0UL)
#endif
    {
    }

ParticipantProxyData::ParticipantProxyData(const ParticipantProxyData& pdata)
    : m_protocolVersion(pdata.m_protocolVersion)
    , m_guid(pdata.m_guid)
    , m_VendorId(pdata.m_VendorId)
    , m_expectsInlineQos(pdata.m_expectsInlineQos)
    , m_availableBuiltinEndpoints(pdata.m_availableBuiltinEndpoints)
    , metatraffic_locators(pdata.metatraffic_locators)
    , default_locators(pdata.default_locators)
    , m_participantName(pdata.m_participantName)
    , m_key(pdata.m_key)
    , lease_duration_(pdata.lease_duration_)
    , lease_duration_us_(pdata.lease_duration_us_)
#if HAVE_SECURITY
    , identity_token_(pdata.identity_token_)
    , permissions_token_(pdata.permissions_token_)
    , security_attributes_(pdata.security_attributes_)
    , plugin_security_attributes_(pdata.plugin_security_attributes_)
#endif
    , m_properties(pdata.m_properties)
    , m_userData(pdata.m_userData)
    // This method is only called from SecurityManager when a new participant is discovered and the
    // corresponding DiscoveredParticipantInfo struct is created. Only participant info is used,
    // so there is no need to copy m_readers and m_writers
    {
    }

bool ParticipantProxyData::writeToCDRMessage(CDRMessage_t* msg, bool write_encapsulation)
{
    std::lock_guard<std::recursive_mutex> lock(ppd_mutex_);

    if (write_encapsulation)
    {
        if (!ParameterList::writeEncapsulationToCDRMsg(msg)) return false;
    }

    {
        ParameterProtocolVersion_t p(PID_PROTOCOL_VERSION,4);
        p.protocolVersion = this->m_protocolVersion;
        if (!p.addToCDRMessage(msg)) return false;
    }
    {
        ParameterVendorId_t p(PID_VENDORID,4);
        p.vendorId[0] = this->m_VendorId[0];
        p.vendorId[1] = this->m_VendorId[1];
        if (!p.addToCDRMessage(msg)) return false;
    }
    if(this->m_expectsInlineQos)
    {
        ParameterBool_t p(PID_EXPECTS_INLINE_QOS, PARAMETER_BOOL_LENGTH, m_expectsInlineQos);
        if (!p.addToCDRMessage(msg)) return false;
    }
    {
        ParameterGuid_t p(PID_PARTICIPANT_GUID, PARAMETER_GUID_LENGTH, m_guid);
        if (!p.addToCDRMessage(msg)) return false;
    }
    for(const Locator_t& it : metatraffic_locators.multicast)
    {
        ParameterLocator_t p(PID_METATRAFFIC_MULTICAST_LOCATOR, PARAMETER_LOCATOR_LENGTH, it);
        if (!p.addToCDRMessage(msg)) return false;
    }
    for(const Locator_t& it : metatraffic_locators.unicast)
    {
        ParameterLocator_t p(PID_METATRAFFIC_UNICAST_LOCATOR, PARAMETER_LOCATOR_LENGTH, it);
        if (!p.addToCDRMessage(msg)) return false;
    }
    for(const Locator_t& it : default_locators.unicast)
    {
        ParameterLocator_t p(PID_DEFAULT_UNICAST_LOCATOR, PARAMETER_LOCATOR_LENGTH, it);
        if (!p.addToCDRMessage(msg)) return false;
    }
    for(const Locator_t& it : default_locators.multicast)
    {
        ParameterLocator_t p(PID_DEFAULT_MULTICAST_LOCATOR, PARAMETER_LOCATOR_LENGTH, it);
        if (!p.addToCDRMessage(msg)) return false;
    }
    {
        ParameterTime_t p(PID_PARTICIPANT_LEASE_DURATION, PARAMETER_TIME_LENGTH);
        p.time = lease_duration_;
        if (!p.addToCDRMessage(msg)) return false;
    }
    {
        ParameterBuiltinEndpointSet_t p(PID_BUILTIN_ENDPOINT_SET, PARAMETER_BUILTINENDPOINTSET_LENGTH);
        p.endpointSet = m_availableBuiltinEndpoints;
        if (!p.addToCDRMessage(msg)) return false;
    }

    if(m_participantName.size() > 0)
    {
        ParameterString_t p(PID_ENTITY_NAME, 0, m_participantName);
        if (!p.addToCDRMessage(msg)) return false;
    }

    if(this->m_userData.size()>0)
    {
        UserDataQosPolicy p;
        p.setDataVec(m_userData);
        if (!p.addToCDRMessage(msg)) return false;
    }

    if(this->m_properties.properties.size()>0)
    {
        ParameterPropertyList_t p(m_properties);
        if (!p.addToCDRMessage(msg)) return false;
    }

#if HAVE_SECURITY
    if(!this->identity_token_.class_id().empty())
    {
        ParameterToken_t p(PID_IDENTITY_TOKEN, 0);
        p.token = identity_token_;
        if (!p.addToCDRMessage(msg)) return false;
    }

    if(!this->permissions_token_.class_id().empty())
    {
        ParameterToken_t p(PID_PERMISSIONS_TOKEN, 0);
        p.token = permissions_token_;
        if (!p.addToCDRMessage(msg)) return false;
    }

    if ((this->security_attributes_ != 0UL) || (this->plugin_security_attributes_ != 0UL))
    {
        ParameterParticipantSecurityInfo_t p;
        p.security_attributes = this->security_attributes_;
        p.plugin_security_attributes = this->plugin_security_attributes_;
        if (!p.addToCDRMessage(msg)) return false;
    }
#endif

    return CDRMessage::addParameterSentinel(msg);
}

bool ParticipantProxyData::readFromCDRMessage(
        CDRMessage_t* msg,
        bool use_encapsulation,
        const NetworkFactory& network)
{
    std::lock_guard<std::recursive_mutex> lock(ppd_mutex_);

    auto param_process = [this, &network](const Parameter_t* param)
    {
        switch (param->Pid)
        {
            case PID_KEY_HASH:
            {
                const ParameterKey_t* p = dynamic_cast<const ParameterKey_t*>(param);
                assert(p != nullptr);
                GUID_t guid;
                iHandle2GUID(guid, p->key);
                this->m_guid = guid;
                this->m_key = p->key;
                break;
            }
            case PID_PROTOCOL_VERSION:
            {
                const ParameterProtocolVersion_t* p = dynamic_cast<const ParameterProtocolVersion_t*>(param);
                assert(p != nullptr);
                if (p->protocolVersion.m_major < c_ProtocolVersion.m_major)
                {
                    return false;
                }
                this->m_protocolVersion = p->protocolVersion;
                break;
            }
            case PID_VENDORID:
            {
                const ParameterVendorId_t* p = dynamic_cast<const ParameterVendorId_t*>(param);
                assert(p != nullptr);
                this->m_VendorId[0] = p->vendorId[0];
                this->m_VendorId[1] = p->vendorId[1];
                break;
            }
            case PID_EXPECTS_INLINE_QOS:
            {
                const ParameterBool_t* p = dynamic_cast<const ParameterBool_t*>(param);
                assert(p != nullptr);
                this->m_expectsInlineQos = p->value;
                break;
            }
            case PID_PARTICIPANT_GUID:
            {
                const ParameterGuid_t* p = dynamic_cast<const ParameterGuid_t*>(param);
                assert(p != nullptr);
                this->m_guid = p->guid;
                this->m_key = p->guid;
                break;
            }
            case PID_METATRAFFIC_MULTICAST_LOCATOR:
            {
                const ParameterLocator_t* p = dynamic_cast<const ParameterLocator_t*>(param);
                assert(p != nullptr);
                Locator_t temp_locator;
                if (network.transform_remote_locator(p->locator, temp_locator))
                {
                    metatraffic_locators.add_multicast_locator(temp_locator);
                }
                break;
            }
            case PID_METATRAFFIC_UNICAST_LOCATOR:
            {
                const ParameterLocator_t* p = dynamic_cast<const ParameterLocator_t*>(param);
                assert(p != nullptr);
                Locator_t temp_locator;
                if (network.transform_remote_locator(p->locator, temp_locator))
                {
                    metatraffic_locators.add_unicast_locator(temp_locator);
                }
                break;
            }
            case PID_DEFAULT_UNICAST_LOCATOR:
            {
                const ParameterLocator_t* p = dynamic_cast<const ParameterLocator_t*>(param);
                assert(p != nullptr);
                Locator_t temp_locator;
                if (network.transform_remote_locator(p->locator, temp_locator))
                {
                    default_locators.add_unicast_locator(temp_locator);
                }
                break;
            }
            case PID_DEFAULT_MULTICAST_LOCATOR:
            {
                const ParameterLocator_t* p = dynamic_cast<const ParameterLocator_t*>(param);
                assert(p != nullptr);
                Locator_t temp_locator;
                if (network.transform_remote_locator(p->locator, temp_locator))
                {
                    default_locators.add_multicast_locator(temp_locator);
                }
                break;
            }
            case PID_PARTICIPANT_LEASE_DURATION:
            {
                const ParameterTime_t* p = dynamic_cast<const ParameterTime_t*>(param);
                assert(p != nullptr);
                lease_duration_ = p->time.to_duration_t();
                lease_duration_us_ = std::chrono::microseconds(TimeConv::Duration_t2MicroSecondsInt64(lease_duration_));
                break;
            }
            case PID_BUILTIN_ENDPOINT_SET:
            {
                const ParameterBuiltinEndpointSet_t* p = dynamic_cast<const ParameterBuiltinEndpointSet_t*>(param);
                assert(p != nullptr);
                this->m_availableBuiltinEndpoints = p->endpointSet;
                break;
            }
            case PID_ENTITY_NAME:
            {
                const ParameterString_t* p = dynamic_cast<const ParameterString_t*>(param);
                assert(p != nullptr);
                this->m_participantName = p->getName();
                break;
            }
            case PID_PROPERTY_LIST:
            {
                const ParameterPropertyList_t* p = dynamic_cast<const ParameterPropertyList_t*>(param);
                assert(p != nullptr);
                this->m_properties = *p;
                break;
            }
            case PID_USER_DATA:
            {
                const UserDataQosPolicy* p = dynamic_cast<const UserDataQosPolicy*>(param);
                assert(p != nullptr);
                this->m_userData = p->getDataVec();
                break;
            }
            case PID_IDENTITY_TOKEN:
            {
#if HAVE_SECURITY
                const ParameterToken_t* p = dynamic_cast<const ParameterToken_t*>(param);
                assert(p != nullptr);
                this->identity_token_ = std::move(p->token);
#else
                logWarning(RTPS_PARTICIPANT, "Received PID_IDENTITY_TOKEN but security is disabled");
#endif
                break;
            }
            case PID_PERMISSIONS_TOKEN:
            {
#if HAVE_SECURITY
                const ParameterToken_t* p = dynamic_cast<const ParameterToken_t*>(param);
                assert(p != nullptr);
                this->permissions_token_ = std::move(p->token);
#else
                logWarning(RTPS_PARTICIPANT, "Received PID_PERMISSIONS_TOKEN but security is disabled");
#endif
                break;
            }
            case PID_PARTICIPANT_SECURITY_INFO:
            {
#if HAVE_SECURITY
                const ParameterParticipantSecurityInfo_t* p =
                    dynamic_cast<const ParameterParticipantSecurityInfo_t*>(param);
                assert(p != nullptr);
                this->security_attributes_ = p->security_attributes;
                this->plugin_security_attributes_ = p->plugin_security_attributes;
#else
                logWarning(RTPS_PARTICIPANT, "Received PID_PARTICIPANT_SECURITY_INFO but security is disabled");
#endif
                break;
            }

            default: break;
        }

        return true;
    };

    uint32_t qos_size;
    clear();
    return ParameterList::readParameterListfromCDRMsg(*msg, param_process, use_encapsulation, qos_size);
}


void ParticipantProxyData::clear()
{
    logInfo(RTPS_PARTICIPANT, m_guid );

    std::lock_guard<std::recursive_mutex> lock(ppd_mutex_);

    m_protocolVersion = ProtocolVersion_t();
    m_guid = GUID_t();
    //set_VendorId_Unknown(m_VendorId);
    m_VendorId = c_VendorId_Unknown;
    m_expectsInlineQos = false;
    m_availableBuiltinEndpoints = 0;
    metatraffic_locators.unicast.clear();
    metatraffic_locators.multicast.clear();
    default_locators.unicast.clear();
    default_locators.multicast.clear();
    m_participantName = "";
    m_key = InstanceHandle_t();
    lease_duration_ = Duration_t();
    lease_duration_us_ = std::chrono::microseconds();
#if HAVE_SECURITY
    identity_token_ = IdentityToken();
    permissions_token_ = PermissionsToken();
    security_attributes_ = 0UL;
    plugin_security_attributes_ = 0UL;
#endif
    m_properties.properties.clear();
    m_properties.length = 0;
    m_userData.clear();

    version_ = SequenceNumber_t();
}

void ParticipantProxyData::copy(const ParticipantProxyData& pdata)
{
    std::lock_guard<std::recursive_mutex> lock(ppd_mutex_);

    m_protocolVersion = pdata.m_protocolVersion;
    m_guid = pdata.m_guid;
    m_VendorId[0] = pdata.m_VendorId[0];
    m_VendorId[1] = pdata.m_VendorId[1];
    m_availableBuiltinEndpoints = pdata.m_availableBuiltinEndpoints;
    metatraffic_locators = pdata.metatraffic_locators;
    default_locators = pdata.default_locators;
    m_participantName = pdata.m_participantName;
    lease_duration_ = pdata.lease_duration_;
    lease_duration_us_ = pdata.lease_duration_us_;
    m_key = pdata.m_key;
    m_properties = pdata.m_properties;
    m_userData = pdata.m_userData;
    version_ = pdata.version_;

    // This method is only called when a new participant is discovered.The destination of the copy
    // will always be a new ParticipantProxyData or one from the pool, so there is no need for
    // m_readers and m_writers to be copied

#if HAVE_SECURITY
    identity_token_ = pdata.identity_token_;
    permissions_token_ = pdata.permissions_token_;
    security_attributes_ = pdata.security_attributes_;
    plugin_security_attributes_ = pdata.plugin_security_attributes_;
#endif
}

bool ParticipantProxyData::updateData(ParticipantProxyData& pdata)
{
    std::lock_guard<std::recursive_mutex> lock(ppd_mutex_);

    metatraffic_locators = pdata.metatraffic_locators;
    default_locators = pdata.default_locators;
    m_properties = pdata.m_properties;
    lease_duration_ = pdata.lease_duration_;
    lease_duration_us_ = pdata.lease_duration_us_;
    m_userData = pdata.m_userData;
#if HAVE_SECURITY
    identity_token_ = pdata.identity_token_;
    permissions_token_ = pdata.permissions_token_;
    security_attributes_ = pdata.security_attributes_;
    plugin_security_attributes_ = pdata.plugin_security_attributes_;
#endif
    return true;
}

void ParticipantProxyData::set_persistence_guid(const GUID_t& guid)
{
    std::lock_guard<std::recursive_mutex> lock(ppd_mutex_);

    // only valid values
    if (guid == c_Guid_Unknown)
    {
        return;
    }

    // generate pair
    std::pair<std::string, std::string> persistent_guid;
    persistent_guid.first = "PID_PERSISTENCE_GUID";

    std::ostringstream data;
    data << guid;
    persistent_guid.second = data.str();

    // if exists replace
    std::vector<std::pair<std::string, std::string>> & props = m_properties.properties;

    std::vector<std::pair<std::string, std::string>>::iterator it =
        std::find_if(
            props.begin(),
            props.end(),
            [&persistent_guid](const std::pair<std::string, std::string> & p)
            {
                return persistent_guid.first == p.first;
            });

    if (it != props.end())
    {
        *it = std::move(persistent_guid);
    }
    else
    {
        // if not exists add
        m_properties.properties.push_back(std::move(persistent_guid));
    }
}

GUID_t ParticipantProxyData::get_persistence_guid() const
{
    std::lock_guard<std::recursive_mutex> lock(ppd_mutex_);

    GUID_t persistent(c_Guid_Unknown);

    const std::vector<std::pair<std::string, std::string>> & props = m_properties.properties;

    std::vector<std::pair<std::string, std::string>>::const_iterator it =
        std::find_if(
            props.cbegin(),
            props.cend(),
            [](const std::pair<std::string, std::string> & p)
            {
                return "PID_PERSISTENCE_GUID" == p.first;
            });

    if (it != props.end())
    {
        std::istringstream in(it->second);
        in >> persistent;
    }

    return persistent;
}

} /* namespace rtps */
} /* namespace fastrtps */
} /* namespace eprosima */
