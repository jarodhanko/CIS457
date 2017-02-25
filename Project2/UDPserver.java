//package project2;

import java.io.Console;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;

/*************************************************************************
* Class: UDPserver
* 
*  The UDP server class creates and manages the server side operations
*  of the file transfer process.
*************************************************************************/
public class UDPserver {

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
		
		UDPserver server = new UDPserver();
		server.runServer();
		
    }
	
	/*********************************************************************
	* Method: runServer
	* 
	* Initializes and runs the UDP server operations. Mostly makes calls
	* to the ServerActions class to perform the desired actions.
	* 
	* @param  (void)
	* @return (void)
	*********************************************************************/
	public void runServer(){
		
		// Variables
		ServerActions sa = new ServerActions();
		Window wd = new Window(); 
		Packet pk = new Packet();
        DatagramSocket socket;
        DatagramPacket inPacket;
        String msg;
        String file;
        int myPort;
        int port;
        InetAddress address;
		
        try{
			
        	// Get the port from the user, set socket port.
        	myPort = sa.getPort();
            socket = new DatagramSocket(myPort);

            // Have server loop to accept more clients after finishing.
            while (true){

                System.out.println("\nRunning...\n");
                
                // Get packet from a connecting client.
                inPacket = sa.getClientMsg(socket);
                
                // Store the clients IP and port.
                address = inPacket.getAddress();
                port = inPacket.getPort();
                
                System.out.println("Client: " + address + " : " + port);
                
                // Get the files in the directory, send to client.
                msg = sa.fileList();
                sa.sendMsg(msg, socket, address, port);
                
                // Get the clients file selection.
                inPacket = sa.getClientMsg(socket);
                file = sa.packetToString(inPacket);
                
                System.out.println("Selected File: " + file);
                
                // Check that the file selected is a valid file.
                if (!sa.fileFound(file)){
                	
                	// The file was not valid, print error message, send error message to client.
                	msg = "ERROR: file not found";
                	System.out.println(msg);
                	sa.sendMsg(msg, socket, address, port);
                }
                else {
                	
                	// The file was found, setup file transfer.
                	sa.setupFileTransfer(file);
                	wd.init();
                	int index = 0;
                	
                	// Begin file transfer loop, continue until no more data to read.
                	while(sa.readData() != -1){
                		
                		//////////////////////////////////////////
                		//***// EXAMPLES - USE AT OWN RISK //***//
                		//////////////////////////////////////////
                		
                		// Create a new packet: pk.
                		pk.newPacket(sa.getData(), sa.getBytesRead());
                		
                		// Add the packet to the window.
                		wd.addPacket(pk);
                		
                		// Send the packet to the client.
                		sa.sendData(pk.getPacket(), socket, address, port);
                		
                		// Get acknowledgement from client.
                		inPacket = sa.getClientMsg(socket);  // Datagram or...
                		msg = sa.packetToString(inPacket);   // String
                		
                		// Update ack for server window, index equals sequence number in ack packet from client.
                		index = ((inPacket.getData())[0] & 0xFF); 
                		wd.setAcknowledged(index);
                		
                		// Slide the window because we received an ack.
                		wd.slide();
                	}
                }  
            }
        }
        catch (Exception e){
            System.out.println("Error\n");
            e.printStackTrace();
        }	
	}
}

