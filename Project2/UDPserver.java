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
import java.util.*;

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
        String file = "";
        int myPort;
        int port = -1;
        InetAddress address = null;
		
        try{
			
        	// Get the port from the user, set socket port.
        	myPort = sa.getPort();
            socket = new DatagramSocket(myPort);
			

            // Have server loop to accept more clients after finishing.
            while (true){

                System.out.println("\nRunning...\n");
                byte initializationNumber = 0;
				while(initializationNumber < 4) {
					switch (initializationNumber){
						case 0: //get packet from connection client;
							System.out.println("Case 0");
							inPacket = sa.getMsg(socket);
							try {
								socket.setSoTimeout(500); 
								if(sa.getPacketSign(inPacket.getData()) == initializationNumber){								
									// Store the clients IP and port.
									address = inPacket.getAddress();
									port = inPacket.getPort();								
									System.out.println("Client: " + address + " : " + port);
									initializationNumber++;
								}
							}catch(Exception ex){
								break;
							}finally{								
								socket.setSoTimeout(0); 
							}
							break;
						case 1: //send fileList
							System.out.println("Case 1");
							try {
								socket.setSoTimeout(500); 
								sa.sendData(sa.signedAck((byte)(initializationNumber - 1)), socket, address, port); //ack previous step							
								// Get the files in the directory, send to client.
								msg = sa.fileList();
								sa.sendData(sa.signPacket(msg.getBytes(), initializationNumber), socket, address, port);								
								inPacket = sa.getMsg(socket); //wait for ack
								if(sa.getPacketSign(inPacket.getData()) == initializationNumber){
									initializationNumber++; //ack successful; continue
								}
							}catch (Exception ex){
								break;
							}finally{								
								socket.setSoTimeout(0); 
							}			
							break;
						case 2: //get file selection
							System.out.println("Case 2");
							try{
								socket.setSoTimeout(500);
								// Get the clients file selection.
								inPacket = sa.getMsg(socket);
								if(sa.getPacketSign(inPacket.getData()) == initializationNumber){
									byte[] designed = sa.designPacket(inPacket.getData());
									file = sa.byteArrayToString(designed).trim();			
									System.out.println("Selected File: " + file);
									if (!sa.fileFound(file)){        									
										msg = "ERROR: file not found";
										System.out.println(msg);
									}else{
										initializationNumber++;
									}
								}								
							}catch(Exception ex){
								break;
							}finally{								
								socket.setSoTimeout(0); 
							}
							break;
						case 3:
							System.out.println("Case 3");
							try{	
								socket.setSoTimeout(500);							
								sa.sendData(sa.signedAck((byte)(initializationNumber - 1)), socket, address, port); //ack previous step	
								inPacket = sa.getMsg(socket);
								if(sa.getPacketSign(inPacket.getData()) == 3){ //wait for ack to continue
									initializationNumber++;
								}
							}catch(Exception ex){
								break;
							}finally{								
								socket.setSoTimeout(0); 
							}
							break;
						default:
							break;
					}
				}
                              	
				// The file was found, setup file transfer.
				sa.setupFileTransfer(file);
				
				boolean exit = false;
				boolean timedOut = false;  
				boolean slid = false;   
				int attempt = 0;
				// Begin file transfer loop, continue until no more data to read and all packets were acknowledged.
				while(!exit || wd.packets().size() != 0){
											
					//read new packets if necessary
					int iterations = wd.maxSize - wd.packets().size();
					for(int i = 0; i < iterations; i++){
						if(sa.readData() != -1){
							
							// Create a new packet
							SlidingPacket pk = new SlidingPacket();
							//System.out.println(Arrays.toString(sa.getData()));
							pk.initialize(sa.getBytesRead(), sa.getData(), false, false);
							if(wd.packets().peekLast() != null)
								pk.setSequenceNumber((byte)((wd.packets().peekLast().seqNumber() + 1) % wd.windowSize));
							else
								pk.setSequenceNumber((byte)wd.slideIndex);
							// Add the packet to the window.
							wd.addPacket(pk);
							if(slid){							
								byte[] checksum = sa.createChecksum(pk.getPacket(false, null));
								byte[] toSend = pk.getPacket(true, checksum);
								System.out.println("Sending: " + toSend[4]);
								//System.out.println(Arrays.toString(toSend));
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

                        
					try{
						socket.setSoTimeout(500);
						// Get acknowledgement from client.
						inPacket = sa.getMsg(socket); 
						byte[] data = inPacket.getData();
						// Determine the seq num from the ack
						byte seqNum = sa.getSeqNum(inPacket);
						System.out.println("Ack: " + seqNum);
						if (seqNum != -1 && ((data[0] == data[1] && data[1] == data[2] && data[2] == data[3]))){
							// Update ack for server window
							wd.setAcknowledged(seqNum);
							attempt = 0;
						}   
						if(attempt > 20){ //client disconnected so acknowledge all packets so it can close
							System.out.println("The client has been lost");
							for(SlidingPacket p: wd.packets()){
								wd.setAcknowledged(p.seqNumber());
							}
						}
					}catch (SocketTimeoutException ex){
						timedOut = true;
						attempt++;
					}finally{
						socket.setSoTimeout(0);
					}

										
					
					System.out.println("slide");
					// Try to slide the window because we received an ack (if corrupted the slide won't do anything).
					slid = wd.slide();
					System.out.println("Exit: " + exit);
				}
				byte[] terminationPacket = {0xF, 0xF, 0xF, 0xF};
				sa.sendData(terminationPacket, socket, address, port);
            }
        }
        catch (Exception e){
            System.out.println("Error\n");
            e.printStackTrace();
        }	
	}
}

