// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The purprose of SessionManager is to facilitate creation of chromotocol
// sessions. Both host and client use it to establish chromotocol
// sessions. JingleChromotocolServer implements this inteface using
// libjingle.
//
// OUTGOING SESSIONS
// Connect() must be used to create new session to a remote host. The
// returned sessionion is initially in INITIALIZING state. Later state is
// changed to CONNECTED if the session is accepted by the host or CLOSED
// if the session is rejected.
//
// INCOMING SESSIONS
// The IncomingSessionCallback is called when a client attempts to connect.
// The callback function decides whether the session should be accepted or
// rejected.
//
// SESSION OWNERSHIP AND SHUTDOWN
// SessionManager owns all Chromotocol Session it creates. The server
// must not be closed while sessions created by the server are still in use.
// When shutting down the Close() method for the sessionion and the server
// objects must be called in the following order: Session,
// SessionManager, JingleClient. The same order must be followed in the case
// of rejected and failed sessions.
//
// PROTOCOL VERSION NEGOTIATION
// When client connects to a host it sends a session-initiate stanza with list
// of supported configurations for each channel. If the host decides to accept
// session, then it selects configuration that is supported by both sides
// and then replies with the session-accept stanza that contans selected
// configuration. The configuration specified in the session-accept is used
// for the session.
//
// The CandidateSessionConfig class represents list of configurations
// supported by an endpoint. The |candidate_config| argument in the Connect()
// specifies configuration supported on the client side. When the host receives
// session-initiate stanza, the IncomingSessionCallback is called. The
// configuration sent in the session-intiate staza is available via
// ChromotocolConnnection::candidate_config(). If an incoming session is
// being accepted then the IncomingSessionCallback callback function must
// select session configuration and then set it with Session::set_config().

#ifndef REMOTING_PROTOCOL_SESSION_MANAGER_H_
#define REMOTING_PROTOCOL_SESSION_MANAGER_H_

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/threading/non_thread_safe.h"
#include "remoting/protocol/session.h"

class Task;

namespace crypto {
class RSAPrivateKey;
}  // namespace base

namespace net {
class X509Certificate;
}  // namespace net

namespace remoting {

class SignalStrategy;

namespace protocol {

// Generic interface for Chromoting session manager.
class SessionManager : public base::NonThreadSafe {
 public:
  SessionManager() { }
  virtual ~SessionManager() { }

  enum IncomingSessionResponse {
    ACCEPT,
    INCOMPATIBLE,
    DECLINE,
  };

  // IncomingSessionCallback is called when a new session is
  // received. If the callback decides to accept the session it should
  // set the second argument to ACCEPT. Otherwise it should set it to
  // DECLINE, or INCOMPATIBLE. INCOMPATIBLE indicates that the session
  // has incompatible configuration, and cannot be accepted.  If the
  // callback accepts session then it must also set configuration for
  // the new session using Session::set_config(). The callback must
  // take ownership of the session if it accepts connection.
  typedef Callback2<Session*, IncomingSessionResponse*>::Type
      IncomingSessionCallback;

  // Initializes the session client. Doesn't accept ownership of the
  // |signal_strategy|. Close() must be called _before_ the |session_manager|
  // is destroyed.
  // If this object is used in server mode, then |private_key| and
  // |certificate| are used to establish a secured communication with the
  // client. It will also take ownership of these objects.
  // In case this is used in client mode, pass in NULL for both private key and
  // certificate.
  virtual void Init(const std::string& local_jid,
                    SignalStrategy* signal_strategy,
                    IncomingSessionCallback* incoming_session_callback,
                    crypto::RSAPrivateKey* private_key,
                    scoped_refptr<net::X509Certificate> certificate) = 0;

  // Tries to create a session to the host |jid|.
  //
  // |host_jid| is the full jid of the host to connect to.
  // |host_public_key| is used to for authentication.
  // |client_oauth_token| is a short-lived OAuth token identify the client.
  // |config| contains the session configurations that the client supports.
  // |state_change_callback| is called when the connection state changes.
  //
  // This function may be called from any thread. The |state_change_callback|
  // is invoked on the network thread.
  //
  // Ownership of the |config| is passed to the new session.
  virtual Session* Connect(
      const std::string& host_jid,
      const std::string& host_public_key,
      const std::string& client_token,
      CandidateSessionConfig* config,
      Session::StateChangeCallback* state_change_callback) = 0;

  // Close session manager and all current sessions. No callbacks are
  // called after this method returns.
  virtual void Close() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionManager);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_SESSION_MANAGER_H_
