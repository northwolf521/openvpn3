//    OpenVPN -- An application to securely tunnel IP networks
//               over a single port, with support for SSL/TLS-based
//               session authentication and key exchange,
//               packet encryption, packet authentication, and
//               packet compression.
//
//    Copyright (C) 2012-2015 OpenVPN Technologies, Inc.
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU Affero General Public License Version 3
//    as published by the Free Software Foundation.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU Affero General Public License for more details.
//
//    You should have received a copy of the GNU Affero General Public License
//    along with this program in the COPYING file.
//    If not, see <http://www.gnu.org/licenses/>.


#ifndef OPENVPN_COMMON_ACTIONTHREAD_H
#define OPENVPN_COMMON_ACTIONTHREAD_H

#include <thread>

#include <openvpn/common/rc.hpp>
#include <openvpn/common/action.hpp>
#include <openvpn/log/logthread.hpp>

namespace openvpn {

  class ActionThread : public RC<thread_safe_refcount>
  {
  public:
    typedef RCPtr<ActionThread> Ptr;

    struct Notify
    {
      virtual void action_thread_finished(const ActionThread* self, bool status) = 0;
    };

    ActionThread(asio::io_service& io_service_arg,
		 const ActionList::Ptr& action_list,
		 Notify* completion_handler_arg)
      : io_service(io_service_arg),
	thread(nullptr),
	actions(action_list),
	completion_handler(completion_handler_arg)
    {
      if (actions)
	thread = new std::thread(&ActionThread::thread_func, this);
    }

    void stop(const bool halt)
    {
      if (thread)
	{
	  if (halt)
	    actions->halt();
	  thread->join();
	  delete thread;
	  thread = nullptr;
	  // Necessary because no guarantee that completion_handler
	  // obj will remain in scope during io_service.post delay.
	  completion_handler = nullptr;
	}
    }

    virtual ~ActionThread()
    {
      stop(true);
    }

  private:
    void completion_post(bool status)
    {
      Notify* n = completion_handler;
      completion_handler = nullptr;
      if (n)
	n->action_thread_finished(this, status);
    }

    void thread_func()
    {
      Log::Context logctx(logwrap);
      bool status = false;
      try {
	OPENVPN_LOG("START THREAD...");
	status = actions->execute();
	OPENVPN_LOG("END THREAD");
      }
      catch (const std::exception& e)
	{
	  OPENVPN_LOG("ActionThread Exception: " << e.what());
	}
      io_service.post([self=Ptr(this), status]()
                      {
                        self->completion_post(status);
                      });
    }

    asio::io_service& io_service;
    std::thread* thread;
    ActionList::Ptr actions;       // actions to execute in child thread
    Notify* completion_handler;    // completion handler
    Log::Context::Wrapper logwrap; // used to carry forward the log context from parent thread
  };

}

#endif
