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
 * @file ThroughputSubscriber.h
 *
 */

#ifndef THROUGHPUTSUBSCRIBER_H_
#define THROUGHPUTSUBSCRIBER_H_

#include <chrono>
#include <condition_variable>
#include <fstream>
#include <iostream>

#include <asio.hpp>
#include <fastdds/dds/publisher/DataWriterListener.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/subscriber/DataReaderListener.hpp>
#include <fastdds/dds/subscriber/qos/DataReaderQos.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastrtps/rtps/attributes/PropertyPolicy.h>
#include <fastrtps/types/DynamicData.h>
#include <fastrtps/types/DynamicDataFactory.h>
#include <fastrtps/types/DynamicPubSubType.h>
#include <fastrtps/types/DynamicType.h>
#include <fastrtps/types/DynamicTypeBuilder.h>
#include <fastrtps/types/DynamicTypeBuilderFactory.h>
#include <fastrtps/types/DynamicTypeBuilderPtr.h>
#include <fastrtps/types/MemberDescriptor.h>
#include <fastrtps/types/TypeDescriptor.h>
#include "ThroughputTypes.hpp"

class ThroughputSubscriber
{
public:

    ThroughputSubscriber(
            bool reliable,
            uint32_t pid,
            bool hostname,
            const eprosima::fastrtps::rtps::PropertyPolicy& part_property_policy,
            const eprosima::fastrtps::rtps::PropertyPolicy& property_policy,
            const std::string& xml_config_file,
            bool dynamic_types,
            int forced_domain);

    virtual ~ThroughputSubscriber();

    bool ready();

    void run();

private:

    void process_message();

    // Entities
    eprosima::fastdds::dds::DomainParticipant* participant_;
    eprosima::fastdds::dds::Subscriber* subscriber_;
    eprosima::fastdds::dds::DataReader* data_reader_;
    eprosima::fastdds::dds::Publisher* publisher_;
    eprosima::fastdds::dds::DataWriter* command_writer_;
    eprosima::fastdds::dds::DataReader* command_reader_;

    // Time
    std::chrono::steady_clock::time_point t_start_;
    std::chrono::steady_clock::time_point t_end_;
    std::chrono::duration<double, std::micro> t_overhead_;

    // Test synchronization
    std::mutex command_mutex_;
    // Block data input processing in Listeners
    std::mutex data_mutex_;
    std::condition_variable command_discovery_cv_;
    std::condition_variable data_discovery_cv_;
    uint32_t command_discovery_count_;
    uint32_t data_discovery_count_;

    // Data and commands
    ThroughputCommandDataType throuput_command_type_;
    // Static Data
    ThroughputDataType* throughput_data_type_;
    ThroughputType* throughput_type_;
    // Dynamic Data
    eprosima::fastrtps::types::DynamicData* dynamic_data_type_;
    eprosima::fastrtps::types::DynamicPubSubType dynamic_pub_sub_type_;
    eprosima::fastrtps::types::DynamicType_ptr dynamic_type_;
    // QoS Profiles
    eprosima::fastdds::dds::DataReaderQos dr_qos_;

    // Flags
    bool dynamic_data_ = false;
    bool ready_;
    int stop_count_;  //! 0 - Continuing test, 1 - End of a test, 2 - Finish application

    // Test configuration
    uint32_t data_size_;
    uint32_t demand_;
    int forced_domain_;

    // Files
    std::string xml_config_file_;

    // Data listener
    class DataReaderListener : public eprosima::fastdds::dds::DataReaderListener
    {
    public:

        DataReaderListener(
                ThroughputSubscriber& throughput_subscriber);

        virtual ~DataReaderListener();

        void on_subscription_matched(
                eprosima::fastdds::dds::DataReader* reader,
                const eprosima::fastdds::dds::SubscriptionMatchedStatus& info) override;

        void on_data_available(
                eprosima::fastdds::dds::DataReader* reader) override;

        void reset();

        void save_numbers();

        uint32_t saved_last_seq_num_;
        uint32_t saved_lost_samples_;

    private:

        ThroughputSubscriber& throughput_subscriber_;
        uint32_t last_seq_num_;
        uint32_t lost_samples_;
        bool first_;
        eprosima::fastdds::dds::SampleInfo info_;
    }
    data_reader_listener_;

    // Command listeners
    class CommandReaderListener : public eprosima::fastdds::dds::DataReaderListener
    {
    public:

        CommandReaderListener(
                ThroughputSubscriber& throughput_subscriber);

        virtual ~CommandReaderListener();

        ThroughputSubscriber& throughput_subscriber_;
        ThroughputCommandType command_type_;
        eprosima::fastdds::dds::SampleInfo info_;

        void on_subscription_matched(
                eprosima::fastdds::dds::DataReader* reader,
                const eprosima::fastdds::dds::SubscriptionMatchedStatus& info) override;

        void on_data_available(
                eprosima::fastdds::dds::DataReader* reader) override;

        void save_numbers();

    private:

        CommandReaderListener& operator =(
                const CommandReaderListener&);
    }
    command_reader_listener_;

    class CommandWriterListener : public eprosima::fastdds::dds::DataWriterListener
    {
    public:

        CommandWriterListener(
                ThroughputSubscriber& throughput_subscriber);

        virtual ~CommandWriterListener();

        ThroughputSubscriber& throughput_subscriber_;

        void on_publication_matched(
                eprosima::fastdds::dds::DataWriter* writer,
                const eprosima::fastdds::dds::PublicationMatchedStatus& info) override;

    private:

        CommandWriterListener& operator =(
                const CommandWriterListener&);
    }
    command_writer_listener_;
};
#endif /* THROUGHPUTSUBSCRIBER_H_ */
