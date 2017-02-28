//package project2;

import java.io.Console;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.io.*;
import java.net.*;
import java.nio.*;
import java.nio.channels.*;

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
		SlidingWindow wd = new SlidingWindow(); 
		//Packet pk = new Packet();
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
					
					boolean exit = false;
					boolean timedOut = false;  
					boolean slid = false;              	
                	// Begin file transfer loop, continue until no more data to read and all packets were acknowledged.
                	while(!exit || wd.packets().size() != 0){
                		
                		//////////////////////////////////////////
                		//***// EXAMPLES - USE AT OWN RISK //***//
                		//////////////////////////////////////////
						
						//read new packets if necessary
						int iterations = wd.maxSize - wd.packets().size();
                		for(int i = 0; i < iterations; i++){
							if(sa.readData() != -1){
								
								// Create a new packet: pk.
								//pk.newPacket(sa.getData(), sa.getBytesRead());
								SlidingPacket pk = new SlidingPacket();
								pk.initialize(sa.getBytesRead(), sa.getData(), false, false);
								//byte[] checksum = sa.createChecksum(pk.getPacket(false, null));
								//pk.getPacket(true, checksum);
								if(wd.packets().peekLast() != null)
									pk.setSequenceNumber((byte)((wd.packets().peekLast().seqNumber() + 1) % wd.windowSize));
								else
									pk.setSequenceNumber((byte)0);
								// Add the packet to the window.
								wd.addPacket(pk);
								if(slid){							
									byte[] checksum = sa.createChecksum(pk.getPacket(false, null));
									byte[] toSend = pk.getPacket(true, checksum);
									System.out.println("Sending: " + toSend[4]);
									sa.sendData(toSend, socket, address, port);
									slid = false;
								}								           		
							}else{
								exit = true;
							}							
						}

						if(timedOut){
							System.out.println("Sending Window");
							sa.sendWindow(wd, socket, address, port);
							timedOut = false;
						}
						
                		System.out.println("waiting for ack");


						socket.setSoTimeout(500);                        
						try{
							// Get acknowledgement from client.
							inPacket = sa.getClientMsg(socket); 
		            		// Determine the seq num from the ack
		            		byte seqNum = sa.getSeqNum(inPacket);
							System.out.println("getting seq number: " + seqNum);
		            		if (seqNum != -1 /*&& (inPacket[0] ^ inPacket[1] ^ inPacket[2] ^ inPacket[3] == 0)*/ ){
								// Update ack for server window
								wd.setAcknowledged(seqNum);
		            		}   
						}catch (SocketTimeoutException ex){
							timedOut = true;
						}finally{
							socket.setSoTimeout(0);
						}

                		             		
                		
						System.out.println("slide");
                		// Try to slide the window because we received an ack (if corrupted the slide won't do anything).
                		slid = wd.slide();
						System.out.println("Exit: " + exit);
                	}
					System.exit(0);
                }  
            }
        }
        catch (Exception e){
            System.out.println("Error\n");
            e.printStackTrace();
        }	
	}
}

