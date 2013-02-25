
/***************************************************************************
 *  llsf-fake-robot.cpp - fake a robot
 *
 *  Created: Fri Feb 22 11:55:51 2013
 *  Copyright  2013  Tim Niemueller [www.niemueller.de]
 ****************************************************************************/

/*  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 * - Neither the name of the authors nor the names of its contributors
 *   may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <config/yaml.h>

#include <protobuf_comm/peer.h>
#include <utils/system/argparser.h>

#include <msgs/BeaconSignal.pb.h>


#include <boost/asio.hpp>
#include <boost/date_time.hpp>

using namespace protobuf_comm;
using namespace llsf_msgs;
using namespace fawkes;

static bool quit = false;
static boost::asio::deadline_timer *timer_ = NULL;
std::string name_;
std::string team_;
unsigned long seq_ = 0;
ProtobufBroadcastPeer *peer_ = NULL;

void
signal_handler(const boost::system::error_code& error, int signum)
{
  if (!error) {
    quit = true;

    if (timer_) {
      timer_->cancel();
    }
  }
}

void
handle_error(const boost::system::error_code &error)
{
  printf("Error: %s\n", error.message().c_str());
}

void
handle_message(boost::asio::ip::udp::endpoint &sender,
	       uint16_t component_id, uint16_t msg_type,
	       std::shared_ptr<google::protobuf::Message> msg)
{
  std::shared_ptr<BeaconSignal> b;
  if ((b = std::dynamic_pointer_cast<BeaconSignal>(msg))) {
    printf("Detected robot: %s:%s (seq %lu)\n",
	   b->team_name().c_str(), b->peer_name().c_str(), b->seq());
  }
}


void
handle_timer(const boost::system::error_code& error)
{
  if (! error) {
    boost::posix_time::ptime now(boost::posix_time::microsec_clock::local_time());
    std::shared_ptr<BeaconSignal> signal(new BeaconSignal());
    Time *time = signal->mutable_time();
    boost::posix_time::time_duration const since_epoch =
      now - boost::posix_time::from_time_t(0);

    time->set_sec(static_cast<google::protobuf::int64>(since_epoch.total_seconds()));
    time->set_nsec(
      static_cast<google::protobuf::int64>(since_epoch.fractional_seconds() * 
					   (1000000000/since_epoch.ticks_per_second())));

    signal->set_peer_name(name_);
    signal->set_team_name(team_);
    signal->set_seq(++seq_);
    peer_->send(signal);

    timer_->expires_at(timer_->expires_at()
		      + boost::posix_time::milliseconds(2000));
    timer_->async_wait(handle_timer);
  }

}


int
main(int argc, char **argv)
{
  ArgumentParser argp(argc, argv, "");

  if (argp.num_items() != 2) {
    printf("Usage: %s <name> <team>\n", argv[0]);
    exit(1);
  }

  name_ = argp.items()[0];
  team_ = argp.items()[1];

  llsfrb::Configuration *config = new llsfrb::YamlConfiguration(CONFDIR);
  config->load("config.yaml");

  if (config->exists("/llsfrb/comm/peer-send-port") &&
      config->exists("/llsfrb/comm/peer-recv-port") )
  {
    peer_ = new ProtobufBroadcastPeer(config->get_string("/llsfrb/comm/peer-host"),
				      config->get_uint("/llsfrb/comm/peer-recv-port"),
				      config->get_uint("/llsfrb/comm/peer-send-port"));
  } else {
    peer_ = new ProtobufBroadcastPeer(config->get_string("/llsfrb/comm/peer-host"),
				      config->get_uint("/llsfrb/comm/peer-port"));
  }

  boost::asio::io_service io_service;

  MessageRegister & message_register = peer_->message_register();
  message_register.add_message_type<BeaconSignal>();

  peer_->signal_received().connect(handle_message);
  peer_->signal_error().connect(handle_error);

  // Construct a signal set registered for process termination.
  boost::asio::signal_set signals(io_service, SIGINT, SIGTERM);

  // Start an asynchronous wait for one of the signals to occur.
  signals.async_wait(signal_handler);

  timer_ = new boost::asio::deadline_timer(io_service);
  timer_->expires_from_now(boost::posix_time::milliseconds(2000));
  timer_->async_wait(handle_timer);

  do {
    io_service.run();
    io_service.reset();
  } while (! quit);

  delete timer_;
  delete peer_;
  delete config;

  // Delete all global objects allocated by libprotobuf
  google::protobuf::ShutdownProtobufLibrary();
}