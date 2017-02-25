//package project2;

import java.io.Console;
import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.SocketException;
import java.nio.channels.UnresolvedAddressException;

/*************************************************************************
* Class: UDPclient
* 
*  The UDP client class creates and manages the client side operations
*  of the file transfer process.
*************************************************************************/
public class UDPclient {
	
	
	/*********************************************************************
	* Method: main
	* 
	* The grand daddy of all the methods, the master mind of the whole
	* operation. Simply creates a non-static copy of itself and runs the 
	* new copy.
	* 
	* @param  (String args[]) -Possible arguments. 
	* @return (void)
	*********************************************************************/
	public static void main(String args[]){
		
		UDPclient client = new UDPclient();
		client.runClient();
    }
	
	
	/*********************************************************************
	* Method: runClient
	* 
	* Initializes and runs the UDP client operations. Mostly makes calls
	* to the ClientActions(not finished) class to perform the desired actions.
	* 
	* @param  (void)
	* @return (void)
	*********************************************************************/
	public void runClient(){
		
		// Varibales.
		ClientActions ca = new ClientActions();
		DatagramPacket inPacket;
		DatagramSocket socket = null;;
		String msg;
		String addr;
		InetAddress address = null;
		int port = -1;
		boolean success = false;
        
		// Allow user to continue attempts to connect to server.
		while(!success){
        addr = ca.getIP();
        port = ca.getPort();

        	// Catch all exceptions for IP/port.
			try {
				if(port < 1024 || port > 65535)
					throw new IOException("This port is not valid");
				address = InetAddress.getByName(addr);
				success = true;
			}catch(UnresolvedAddressException ex){
				System.out.println("The specified IP address was invalid or no server was available");
			}catch(NumberFormatException ex){
				System.out.println("The port must be a valid integer value");
			}catch(IOException ex){
				System.out.println("The specified port number or IP Address was invalid");		
			}
		}
		
		// Init the socket.
		try {
			socket = new DatagramSocket();
		} catch (SocketException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
        
		// Send a signal to the server;
		msg = "";
		ca.sendMsg(msg, socket, address, port);
        
		// Get the file list from the server. Print.
        inPacket = ca.getServerMsg(socket);
        msg = ca.packetToString(inPacket);
        System.out.println(msg);
        
        // User chooses file, send choice to server.
        Console cons = System.console();
        msg = cons.readLine();
        ca.sendMsg(msg, socket, address, port);
		
		// File transfer loop.
        while(true){
        	
        }
		
	}
	

}
