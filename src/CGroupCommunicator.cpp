#include <QDataStream>

#include "CGroupCommunicator.h"


#include "defines.h"

#include "CConfigurator.h"
#include "utils.h"
#include "CGroup.h"
#include "CGroupServer.h"
#include "CGroupClient.h"


CGroupCommunicator::CGroupCommunicator(int _type, QObject *parent):
	QObject(parent)
{
	type = Off;
	peer = new CGroupDraftConnection;
	changeType(_type);
}

CGroupCommunicator::~CGroupCommunicator()
{
	
}

void CGroupCommunicator::changeType(int newState) {
	if (type == newState)
		return;

	if (type == Client) {
		peer->deleteLater();
	} else 
		delete peer;

	type = newState;

	print_debug(DEBUG_GROUP, "Changing the Type of the GroupManager: new Type %i.", newState);
	switch (newState) {
		case Server:
			peer = new CGroupServer(conf->getGroupManagerLocalPort(), this);
			break;
		case Client:
			peer = new CGroupClient(conf->getGroupManagerHost(), conf->getGroupManagerRemotePort(), this);
			break;
		default:
			break;
	}
	
}

void CGroupCommunicator::connectionStateChanged(CGroupClient *connection)
{
//	Closed, Connecting, Connected, Logged, Quiting
	print_debug(DEBUG_GROUP, "CGroupMananger: connection state changed. type: %i", type);
	switch (connection->getConnectionState()) {
		case CGroupClient::Connecting :
			print_debug(DEBUG_GROUP, "Connecting to the remote host.");
			connecting(connection);
			break;
		case CGroupClient::Connected :
			print_debug(DEBUG_GROUP, "Connection established.");
			connectionEstablished(connection);
			break;
		case CGroupClient::Closed :
			print_debug(DEBUG_GROUP, "Connection closed.");
			connectionClosed(connection);
			break;
		case CGroupClient::Quiting :
			print_debug(DEBUG_GROUP, "Closing the socket. Quiting");
			//connectionClosed(connection);
			break;
		default:
			printf("Some state change...\r\n");
			break;
	}
}


void CGroupCommunicator::connecting(CGroupClient *connection)
{
	print_debug(DEBUG_GROUP, "CONNECTING. Type %i", type);
	if (type == Client) {
		
	} else if (type == Server) {
		
	}
}

void CGroupCommunicator::connectionEstablished(CGroupClient *connection)
{
	if (type == Client) {
		connection->setProtocolState(CGroupClient::AwaitingLogin);
	} 
	if (type == Server) {
		connection->setProtocolState(CGroupClient::AwaitingLogin);
		sendMessage(connection, REQ_LOGIN);
	}
}

void CGroupCommunicator::connectionClosed(CGroupClient *connection)
{
	if (type == Client) {
		changeType(Off);
	} else if (type == Server) {
		getGroup()->connectionClosed("");
		
		CGroupServer *server = (CGroupServer *) peer;
		server->connectionClosed(connection);
	}
	
}

void CGroupCommunicator::errorInConnection(CGroupClient *connection)
{
	QString str;

	switch(connection->error()) {
		case QAbstractSocket::ConnectionRefusedError:	
			getGroup()->connectionRefused(connection->peerName() + " port "+ 
					connection->peerPort() );
			break;
		case QAbstractSocket::RemoteHostClosedError:
			//connectionClosed(connection);
			getGroup()->connectionError("Remote host closed the connection");
			break;
		case QAbstractSocket::HostNotFoundError:
			str = "Host not found";
			str += connection->peerName();
			getGroup()->connectionRefused( str );
			break;
		case QAbstractSocket::SocketAccessError:
			getGroup()->connectionError("Socket Access Error");
			break;
		case QAbstractSocket::SocketResourceError:
			getGroup()->connectionError("Socket Resource Error");
			break;
		case QAbstractSocket::SocketTimeoutError:
			getGroup()->connectionError("Socket Timeout Error");
			break;
		case QAbstractSocket::DatagramTooLargeError:
			getGroup()->connectionError("Diagram Too Large Error");
			break;
		case QAbstractSocket::NetworkError:
			getGroup()->connectionError("Network Error");
			break;
		case QAbstractSocket::AddressInUseError:
		case QAbstractSocket::SocketAddressNotAvailableError:
			getGroup()->connectionRefused("Network Error");
			break;
		case QAbstractSocket::UnsupportedSocketOperationError:
		case QAbstractSocket::ProxyAuthenticationRequiredError:
		case QAbstractSocket::UnknownSocketError:
		case QAbstractSocket::UnfinishedSocketOperationError:
			getGroup()->connectionError("Network Error ");
			break;
	}
	
}

void CGroupCommunicator::serverStartupFailed()
{
	CGroupServer *server = (CGroupServer *) peer;
	getGroup()->serverStartupFailed(server->errorString());
	changeType(Off);
}

//
// COmmunication protocol switches and logic
//

// the core of the protocol
void CGroupCommunicator::incomingData(CGroupClient *conn)
{
	QByteArray data;
	QByteArray blob;
	int message;
	int index;

	while (conn->bytesAvailable()) {
		data = conn->readLine();
		print_debug(DEBUG_GROUP, "ReceivedData. [conn: %i, Data: %s]", 
						(int) conn->socketDescriptor(),
						(const char *) data);

		index = data.indexOf(' ');
		
		QByteArray copy = data.left(index);
		message = copy.toInt();

		blob = data.right( data.size() - index - 1);
		
		print_debug(DEBUG_GROUP, "Datagram arrived. Message : %i, Blob: %s", message, (const char *) blob);

		if (type == Client)
			retrieveDataClient(conn, message, blob);
		if (type == Server)
			retrieveDataServer(conn, message, blob);

	}

}

//
// ******************** C L I E N T   S I D E ******************
//
// Client side of the communication protocol
void CGroupCommunicator::retrieveDataClient(CGroupClient *conn, int message, QByteArray data)
{
	switch (conn->getConnectionState()) {
		//Closed, Connecting, Connected, Quiting
		case CGroupClient::Connected:
			// AwaitingLogin, AwaitingInfo, Logged 

			if (conn->getProtocolState() == CGroupClient::AwaitingLogin) {
				// Login state. either REQ_LOGIN or ACK should come
				if (message == REQ_LOGIN) {
					sendLoginInformation(conn);
				} else if (message == ACK) {
					// aha! logged on!
					sendMessage(conn, REQ_INFO);
					conn->setProtocolState(CGroupClient::AwaitingInfo);
				} else if (message == STATE_KICKED) {
					// woops
					getGroup()->gotKicked(data);
				} else {
					// ERROR: unexpected message marker!
					// try to ignore?
					print_debug(DEBUG_GROUP, "(AwaitingLogin) Unexpected message marker. Trying to ignore.");
				}
				
			} else if (conn->getProtocolState() == CGroupClient::AwaitingInfo) {
				// almost connected. awaiting full information about the connection
				if (message == UPDATE_CHAR) {
					parseGroupInformation(conn, data);
				} else if (message == STATE_LOGGED) {
					conn->setProtocolState(CGroupClient::Logged);
				} else if (message == REQ_ACK) {
					sendMessage(conn, ACK);
				} else {
					// ERROR: unexpected message marker!
					// try to ignore?
					print_debug(DEBUG_GROUP, "(AwaitingInfo) Unexpected message marker. Trying to ignore.");
				}
				
			} else if (conn->getProtocolState() == CGroupClient::Logged) {
				// usual update situation. receive update, unpack, apply.
				if (message == ADD_CHAR) {

				} else if (message == REMOVE_CHAR) {
					
				} else if (message == UPDATE_CHAR) {
					
				} else if (message == GTELL) {
					
				} else if (message == REQ_ACK) {
					sendMessage(conn, ACK);
				} else {
					// ERROR: unexpected message marker!
					// try to ignore?
					print_debug(DEBUG_GROUP, "(AwaitingInfo) Unexpected message marker. Trying to ignore.");
				}
				
				
			} 
			
			break;
		case CGroupClient::Closed:
			print_debug(DEBUG_GROUP, "(Closed) Data arrival during wrong connection state.");
			break;
		case CGroupClient::Connecting:
			print_debug(DEBUG_GROUP, "(Connecting) Data arrival during wrong connection state.");
			break;
		case CGroupClient::Quiting:
			print_debug(DEBUG_GROUP, "(Quiting) Data arrival during wrong connection state.");
			break;
	}
}

//
// ******************** S E R V E R   S I D E ******************
//
// Server side of the communication protocol
void CGroupCommunicator::retrieveDataServer(CGroupClient *conn, int message, QByteArray data)
{
	
	switch (conn->getConnectionState()) {
		//Closed, Connecting, Connected, Quiting
		case CGroupClient::Connected:
			// AwaitingLogin, AwaitingInfo, Logged 

			// ---------------------- AwaitingLogin  --------------------
			if (conn->getProtocolState() == CGroupClient::AwaitingLogin) {
				// Login state. either REQ_LOGIN or ACK should come
				if (message == UPDATE_CHAR) {
					// aha! parse the data
					conn->setProtocolState(CGroupClient::AwaitingInfo);
					parseLoginInformation(conn, data);
				} else {
					// ERROR: unexpected message marker!
					// try to ignore?
					print_debug(DEBUG_GROUP, "(AwaitingLogin) Unexpected message marker. Trying to ignore.");
				}
				// ---------------------- AwaitingInfo  --------------------
			} else if (conn->getProtocolState() == CGroupClient::AwaitingInfo) {
				// almost connected. awaiting full information about the connection
				if (message == REQ_INFO) {
					sendGroupInformation(conn);
					sendMessage(conn, REQ_ACK);
				} else if (message == ACK) {
					conn->setProtocolState(CGroupClient::Logged);
					sendMessage(conn, STATE_LOGGED);
				} else {
					// ERROR: unexpected message marker!
					// try to ignore?
					print_debug(DEBUG_GROUP, "(AwaitingInfo) Unexpected message marker. Trying to ignore.");
				}
				
				// ---------------------- LOGGED --------------------
			} else if (conn->getProtocolState() == CGroupClient::Logged) {
				// usual update situation. receive update, unpack, apply.
				
				
			} 
			
			break;
			
		case CGroupClient::Closed:
			
			
		case CGroupClient::Connecting:
		case CGroupClient::Quiting:
			print_debug(DEBUG_GROUP, "Data arrival during wrong connection state.");
			break;
	}
}

//
// user functions
//
void CGroupCommunicator::userLoggedOn(CGroupClient *conn)
{
	// TODO: call group-function. 
	// TODO: message all other peers on server side
}

void CGroupCommunicator::userLoggedOff(CGroupClient *conn)
{
	// TODO: call group-function. 
	// TODO: message all other peers on server side
}


//
// Parsers and Senders of information and signals to upper and lower objects
//
void CGroupCommunicator::sendLoginInformation(CGroupClient *conn)
{
	QByteArray info;
	
	// temporary
	
	sendMessage(conn, UPDATE_CHAR, getGroup()->getLocalCharData() );
}

void CGroupCommunicator::parseLoginInformation(CGroupClient *conn, QByteArray data)
{
	print_debug(DEBUG_GROUP, "Login Information arrived %s", (const char *) data);

	if (getGroup()->addChar(data) == true) {
		sendMessage(conn, ACK); 
	} else {
		sendMessage(conn, STATE_KICKED, "The name you picked is already present!");
		conn->close();	// got to make sure this causes the connection closed signal ...
	}
	
	
}

void CGroupCommunicator::sendGroupInformation(CGroupClient *conn)
{
	QByteArray info;

	getGroup()->sendAllCharsData(conn);
	sendMessage(conn, REQ_ACK);
}

void CGroupCommunicator::sendCharUpdate(CGroupClient *conn, QByteArray blob)
{
	sendMessage(conn, UPDATE_CHAR, blob);
}


void CGroupCommunicator::parseGroupInformation(CGroupClient *conn, QByteArray data)
{
	// temporary
	print_debug(DEBUG_GROUP, "Group Information arrived %s", (const char *) data);
	getGroup()->addChar(data);
}


//
// Low level. Message forming and messaging
//
QByteArray CGroupCommunicator::formMessageBlock(int message, QByteArray data)
{
	QByteArray block;
/*
 *     QDataStream out(&block, QIODevice::WriteOnly);

    out.setVersion(QDataStream::Qt_4_0);	
    out << (quint16)0;
    out << message;
    out << data;
    out.device()->seek(0);
    out << (quint16)(block.size() - sizeof(quint16));	
*/
	char buffer[MAX_STR_LEN];

	sprintf(buffer, "%i %s", message, (const char *) data);
	block = buffer;
	print_debug(DEBUG_GROUP, "Message: %s", (const char *) block);

    return block;
}

void CGroupCommunicator::sendMessage(CGroupClient *connection, int message, QByteArray data)
{
	print_debug(DEBUG_GROUP, "Sending message");
	if (connection->write( formMessageBlock(message, data) ) == -1)
		print_debug(DEBUG_GROUP, "Error at attempt of writing the data!");
}
