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
		SlidingWindow window = new SlidingWindow();
        
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
        boolean fileFound = false;
        boolean lastPacket = false;
        boolean corrupted = false;
        int seqNum = 0;
        while(!lastPacket){
        	
        	//grab next packet
        	inPacket = ca.getServerMsg(socket);
			
			//make sure it is not file not found -- only run on initial packet
			if (!fileFound){
				String s = ca.packetToString(inPacket);
				if(s.equals("ERROR: file not found")){
					System.out.println(s);
					break;
				}
				fileFound = true;
				ca.setupFile(msg);
			}
			
			// Check for corruption. FIX ME need a client sliding window.
			corrupted = ca.checkChecksum(inPacket.getData());
			
			// Get the sequence number. FIX ME, is sequence number at index 4?? use seqNum to sort sliding window
			seqNum = ca.getSeqNum(inPacket);
        	
			//check if it is the last packet, index 5-8 holds size??, if size less than 1024, is last packet.
			lastPacket = ca.isLastPacket(inPacket);   // FIX ME, not sure where data size was stored in header, check method to verify.
			
			//write packet
			ca.writeToFile(inPacket.getData());
			
			//acknowledge, FIX ME, not sure where sequence number is in header, index 4?? Double check
			ca.sendAck(inPacket, socket, address, port);
			
        }
        ca.closeFile();
		
	}
	

}
