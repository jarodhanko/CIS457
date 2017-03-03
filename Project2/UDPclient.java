//Jarod Hanko and Norman Cunningham
//3/3/2017

import java.io.Console;
import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.SocketException;
import java.nio.channels.UnresolvedAddressException;
import java.util.LinkedList;
import java.util.*;

import java.io.*;
import java.net.*;
import java.nio.*;
import java.nio.channels.*;
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
		try {
			// Varibales.
			ClientActions ca = new ClientActions();
			DatagramPacket inPacket;
			DatagramSocket socket = null;;
			String msg = "";
			String addr;
			InetAddress address = null;
			int port = -1;
			boolean success = false;
			int attempt = 0;
			String selection = "";
			SlidingWindow window = new SlidingWindow();	
			
			// Init the socket.
			try {
				socket = new DatagramSocket();
			} catch (SocketException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
			
			byte initializationNumber = 0;
			while(initializationNumber < 3) {
				switch (initializationNumber){
					case 0: //send packet to server;
						System.out.println("Case 0");
						try {
							socket.setSoTimeout(500); 
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
								}catch(Exception ex){
									System.out.println("The specified port number or IP Address was invalid");		
								}
							}
							byte[] handshake = ca.signedAck(initializationNumber);
							ca.sendData(handshake, socket, address, port);
							inPacket = ca.getMsg(socket); //wait for ack
							if(ca.getPacketSign(inPacket.getData()) == initializationNumber){								
								initializationNumber++;
							}
						}catch(Exception ex){
							if(attempt > 10){ //attempt to connect 3 times before restarting
								System.out.println("Failed to connect to server");
								success = false;
								attempt = 0;
							}
							attempt++;
						}finally{
							attempt = 0;
							success = false;
							socket.setSoTimeout(0); 
						}
						break;
					case 1: //receive filelist
						System.out.println("Case 1");
						try {	
							socket.setSoTimeout(500); 							
							inPacket = ca.getMsg(socket); //wait for filelist								
							if(ca.getPacketSign(inPacket.getData()) == initializationNumber){
								byte[] designed = ca.designPacket(inPacket.getData());
								msg = ca.byteArrayToString(designed);
								initializationNumber++;
							}
						}catch (Exception ex){
							break;
						}finally{								
							socket.setSoTimeout(0); 
						}			
						break;
					case 2: //send file selection
						System.out.println("Case 2");
						try{
							socket.setSoTimeout(500); 
							ca.sendData(ca.signedAck((byte)(initializationNumber - 1)), socket, address, port); //send ack for previous step
							// Get the clients file selection.								
							if(!success){
								// User chooses file, send choice to server.
								System.out.println(msg);
								System.out.println("Enter a filename to transfer: ");
								Console cons = System.console();
								selection = cons.readLine();
								success = true;
							}
							ca.sendData(ca.signPacket(selection.getBytes(), initializationNumber), socket, address, port);
							inPacket = ca.getMsg(socket);
							if(ca.getPacketSign(inPacket.getData()) == initializationNumber){
								ca.setupFile(selection);
								initializationNumber++;
								attempt = 0;
								success = false;	
							}else{
								if(attempt > 10){
									System.out.println("That file does not exist or the selection failed");
									success = false;
									attempt = 0;
								}else{
									attempt++;
								}
							}							
						}catch(Exception ex){
							//do nothing
						}finally{						
							socket.setSoTimeout(0); 							
						}
						break;
					default:
						break;
				}
			}
			
			
			
			//wait for ack !!
			
			
			//wait for ack file found !!
			
			// File transfer loop.
			boolean fileFound = false;
			boolean lastPacket = false;
			boolean corrupted = false;
			byte seqNum = (byte)0;
			success = false;
			while(!lastPacket || window.packets().size() != 0){
				try{
					socket.setSoTimeout(1000); 
					if(!success){
						System.out.println("sendingAck");
						ca.sendData(ca.signedAck((byte)3), socket, address, port); //continue ack
					}
					System.out.println("Next Packet");
					//grab next packet
					inPacket = null;
					inPacket = ca.getMsg(socket);
					
					corrupted = !ca.checkChecksum(inPacket.getData());
					System.out.println("Corrupted: " + corrupted);
					
					if(!corrupted){					
						seqNum = ca.getSeqNum(inPacket);
						
						//check if it is the last packet, index 5-8 holds size, if size less than 1024, is last packet.
						lastPacket = ca.isLastPacket(inPacket);   
						System.out.println("Seq Num: " +  seqNum + " last packet: " + lastPacket);
						SlidingPacket newPacket = new SlidingPacket();
						if(newPacket.setFromPacket(inPacket.getData(), true)){
							success = true; //don't need to send continue ack anymore since we sucessfully received a packet
							System.out.println("Sending Ack: " + seqNum);
							//acknowledge, always send
							ca.sendAck(seqNum, socket, address, port);
							
							if(window.addPacket(newPacket)){
								System.out.println("add to window");
								for(int i = 0; i < window.maxSize; i++){
									if(window.readyForSlide()){	
										System.out.println("\t \t WRITING: \t \t " + window.packets().peek().seqNumber());
										//write packet data to file
										byte[] temp = Arrays.copyOfRange(window.packets().peek().data(), 0, window.packets().peek().length() - 4);
										ca.writeToFile(temp);
										window.acknowledgeFirst();
										window.slide();
										System.out.println("Slide");
									}else{
										break;
									}
								}
							}
						}else{
							 for(slidingpacket pk: window.packets()){
								 if(!pk.acknowledged() && pk.seqnumber() != -1){
									 byte[] temp = arrays.copyofrange(pk.data(), 0, pk.length() - 4);
									 ca.writetofile(temp);
									 system.out.println("\t \t writing special: \t \t" + pk.seqnumber());
									 window.setacknowledged(pk.seqnumber());
								 }
							}
							window.slide();
							//is a termination packet
							byte[] pk = inPacket.getData();
							if(pk[0] == 0xF && pk[1] == 0xF && pk[2] == 0xF && pk[3] == 0xF && pk[4] == 0xF && pk[5] == 0xF && pk[6] == 0xF && pk[7] == 0xF){
								lastPacket = true;
							}
						}
						attempt = 0; //reset fail counter
					}
				} catch(SocketTimeoutException ex){
					if(attempt > 20){
						System.out.println("The server is unreachable.  Exiting");
						System.exit(0);
					}else{
						attempt++;
					}
					socket.setSoTimeout(0); 
				}
			}
			ca.closeFile();
			socket.setSoTimeout(0); 
		}catch(Exception ex){
			ex.printStackTrace();
		}
		
	}
	

}
