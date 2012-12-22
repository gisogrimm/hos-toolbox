/**
   \file libhos_midi_ctl.cc
   \ingroup apphos
   \brief Classes for matrix mixer MIDI control

Copyright (C) 2011 Giso Grimm

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/
#include "libhos_midi_ctl.h"
#include <pthread.h>

midi_ctl_t::midi_ctl_t(const std::string& cname)
  : seq(NULL),
    b_run_service(false)
{
  //DEBUG(1);
  if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, SND_SEQ_NONBLOCK) < 0)
    throw "Unable to open sequencer.";
  snd_seq_set_client_name(seq, cname.c_str());
  snd_seq_drop_input(seq);
  snd_seq_drop_input_buffer(seq);
  snd_seq_drop_output(seq);
  snd_seq_drop_output_buffer(seq);
  port_in.port = 
    snd_seq_create_simple_port(seq, "control",
			       SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE,
			       SND_SEQ_PORT_TYPE_APPLICATION);
  port_out.port = 
    snd_seq_create_simple_port(seq, "feedback",
			       SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ,
			       SND_SEQ_PORT_TYPE_APPLICATION);
  port_in.client = snd_seq_client_id(seq);
  port_out.client = snd_seq_client_id(seq);
  // todo: error handling!
}


void midi_ctl_t::connect_input(int src_client,int src_port)
{
  //DEBUG(src_client);
  //DEBUG(src_port);
  snd_seq_addr_t src_port_;
  src_port_.client = src_client;
  src_port_.port = src_port;
  snd_seq_port_subscribe_t *subs;
  snd_seq_port_subscribe_alloca(&subs);
  snd_seq_port_subscribe_set_sender(subs, &src_port_);
  snd_seq_port_subscribe_set_dest(subs, &port_in);
  snd_seq_port_subscribe_set_queue(subs, 1);
  snd_seq_port_subscribe_set_time_update(subs, 1);
  snd_seq_port_subscribe_set_time_real(subs, 1);
  snd_seq_subscribe_port(seq, subs);
}

void midi_ctl_t::connect_output(int src_client,int src_port)
{
  //DEBUG(src_client);
  //DEBUG(src_port);
  snd_seq_addr_t src_port_;
  src_port_.client = src_client;
  src_port_.port = src_port;
  snd_seq_port_subscribe_t *subs;
  snd_seq_port_subscribe_alloca(&subs);
  snd_seq_port_subscribe_set_sender(subs, &port_out);
  snd_seq_port_subscribe_set_dest(subs, &src_port_);
  snd_seq_port_subscribe_set_queue(subs, 1);
  snd_seq_port_subscribe_set_time_update(subs, 1);
  snd_seq_port_subscribe_set_time_real(subs, 1);
  snd_seq_subscribe_port(seq, subs);
}

void * midi_ctl_t::service(void* h)
{
  //DEBUG(h);
  ((midi_ctl_t*)h)->service();
  //DEBUG(h);
  return NULL;
}

void midi_ctl_t::start_service()
{
  //DEBUG(b_run_service);
  if( b_run_service )
    return;
  b_run_service = true;
  int err = pthread_create( &srv_thread, NULL, &midi_ctl_t::service, this);
  if( err < 0 )
    throw "pthread_create failed";
  //DEBUG(b_run_service);
}

void midi_ctl_t::stop_service()
{
  //DEBUG(b_run_service);
  if( !b_run_service )
    return;
  b_run_service = false;
  //DEBUG(b_run_service);
  pthread_join( srv_thread, NULL );
  //DEBUG(b_run_service);
}

void midi_ctl_t::service(){
  snd_seq_drop_input(seq);
  snd_seq_drop_input_buffer(seq);
  snd_seq_drop_output(seq);
  snd_seq_drop_output_buffer(seq);
  //DEBUG(b_run_service);
  snd_seq_event_t *ev;
  while(b_run_service ){
    //while( snd_seq_event_input_pending(seq,0) ){
    while( snd_seq_event_input(seq, &ev) >= 0 ){
      if( ev->type == SND_SEQ_EVENT_CONTROLLER ){
	//DEBUG(ev->data.control.channel);
	//DEBUG(ev->data.control.param);
	//DEBUG(ev->data.control.value);
	emit_event(ev->data.control.channel,ev->data.control.param, ev->data.control.value);
      }
    }
    usleep(500);
  }
  //DEBUG(b_run_service);
}

midi_ctl_t::~midi_ctl_t()
{
  //DEBUG(b_run_service);
  //DEBUG(seq);
  snd_seq_close(seq);
  //DEBUG(seq);
}

void midi_ctl_t::send_midi(int channel, int param, int value)
{
  snd_seq_event_t ev;
  snd_seq_ev_clear(&ev);
  snd_seq_ev_set_source(&ev, port_out.port);
  snd_seq_ev_set_subs(&ev);
  snd_seq_ev_set_direct( &ev );
  ev.type = SND_SEQ_EVENT_CONTROLLER;
  ev.data.control.channel = (unsigned char)(channel);
  ev.data.control.param = (unsigned char)(param);
  ev.data.control.value = (unsigned char)(value);
  snd_seq_event_output_direct(seq, &ev);
  snd_seq_drain_output(seq);
  snd_seq_sync_output_queue(seq);
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
