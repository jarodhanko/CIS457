/*************************************************************************
* Author:    Norman Cunningham, Jarod Hanko
* File:      udpclient 
* Version:   5.37
* Task:      Project 2A
* Date Due:  02/21/2017 
*************************************************************************/

import java.io.*;
import java.net.*;
import java.util.Scanner;
import java.util.Arrays;
import java.nio.*;
import java.nio.channels.*;

class udpclient{

    public static void main(String args[]){

        DatagramSocket socket = null;
        DatagramPacket inPacket = null;
        DatagramPacket outPacket = null;
        byte[] inBuf;
        byte[] outBuf;
        String msg = null;
		int port = 0;
        Scanner src = new Scanner(System.in);
		InetAddress address = null;
		boolean success = false;
		while(true){
        try{
			while(!success) {
				try{
					Console cons = System.console();
					String addr = cons.readLine("Please enter an IP address: ");
					port = Integer.parseInt(cons.readLine("Please enter a port number: "));
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

			socket = new DatagramSocket();

            // Send signal to server.
            msg = "";
            outBuf = msg.getBytes();
            outPacket = new DatagramPacket(outBuf, 0, outBuf.length, address, port);
            socket.send(outPacket);
			
            // Recieve available file list from server.
            inBuf = new byte[1024];
            inPacket = new DatagramPacket(inBuf, inBuf.length);
            socket.receive(inPacket);

            // Print the file list.
            String data = new String(inPacket.getData(), 0, inPacket.getLength());
            System.out.println(data);

            // Send the file name.
            String filename = src.nextLine();
            outBuf = filename.getBytes();
            outPacket = new DatagramPacket(outBuf, 0, outBuf.length, address, port);
            socket.send(outPacket);

            // Begin recieve file process.
            data = "";
            byte[] buffer = new byte[65535];
            OutputStream fos = new FileOutputStream("~"+filename);
            
            byte[][] recieved = new byte[5][];
            for(byte[] x : recieved)
                x = null;
            int rcv = 0;
            int counter = 0;
            int counter2 = 0;
            int c = 1;
            byte[] tempBuf;
 
            // Continue to look for packets until finished.
            while(true){
                counter2++;
                
                // Grab the next packet.
                inBuf = new byte[1024];
                inPacket = new DatagramPacket(inBuf, inBuf.length);
                socket.receive(inPacket);

				if(new String(inPacket.getData(), 0, inPacket.getLength()).equals("ERROR: file not found"))
					throw new FileNotFoundException("File was not found.  Please try again");

                // Check header to see if last packet. If so, determine how much data
                // was sent, and resize the buffer accordingly. Write buffer to file.
                //System.out.println(counter);
                if(inBuf[0] == (byte)11){

                    for(int i = 0; i < 5; i++){
                        if(recieved[i] != null){
                            tempBuf = Arrays.copyOfRange(recieved[i], 4, inBuf.length);
                            fos.write(tempBuf);
                        }
                    }

                    String sSize = "";
                    for(int i = 0; i < 3; i++){
                        sSize += Integer.toString((int)inBuf[i+1]);
                    }
                    int iSize = Integer.parseInt(sSize);
                    tempBuf = Arrays.copyOfRange(inBuf, 4, 4+iSize);
                    fos.write(tempBuf);
                    break;
                    
                } else{
					//acknowledge
                    // Get packet number signal server.
                    String signal = Integer.toString((int)inBuf[0]);
                    rcv = Integer.parseInt(signal);
                    System.out.println(counter2);
                    System.out.println(rcv);
                    recieved[rcv] = inBuf;
                    outBuf = new byte[100];
                    outBuf = signal.getBytes();
                    outPacket = new DatagramPacket(outBuf, 0, outBuf.length, address, port);
					socket.send(outPacket);
                }

                for(int i = 0; i < 5; i++){
                    if(recieved[i] != null)
                        counter++;
                }
                if (counter == 5){
                    for(int i = 0; i < 5; i++){
                        tempBuf = Arrays.copyOfRange(recieved[i], 4, inBuf.length);
                        fos.write(tempBuf);
                        recieved[i] = null;
                    }
                }
                counter = 0;
                

            }

            // Close output stream.
            fos.close();
			System.exit(0);
        } catch(FileNotFoundException e){
			//do nothing
		} 
		catch(Exception e)
        {
            System.out.println("\nNetwork Error. Please try again.\n");
            e.printStackTrace();
            System.out.println(e.getMessage());
        }
		}
    }
}
