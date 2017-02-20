/*************************************************************************
* Author:    Norman Cunningham, Jarod Hanko
* File:      udpserver 
* Version:   5.37
* Task:      Project 2A
* Date Due:  02/21/2017 
*************************************************************************/

import java.io.*;
import java.net.*;
import java.nio.*;
import java.nio.channels.*;
import java.util.*;
import static java.lang.Math.abs;

class udpserver{

    public static void main(String args[]){

        DatagramSocket socket = null;
        DatagramPacket inPacket = null;
        DatagramPacket outPacket = null;
        byte[] inBuf;
        byte[] outBuf;
        byte[] header;
        String msg;
        final int PORT = 50000;

        try{

            socket = new DatagramSocket(PORT);
			//socket.setSoTimeout(500);

            while(true){

                System.out.println("\nRunning...\n");

                // Recieve new connection.
                inBuf = new byte[100];
                inPacket = new DatagramPacket(inBuf, inBuf.length);
                socket.receive(inPacket);

                // Store client information.
                int source_port = inPacket.getPort();
                InetAddress source_address = inPacket.getAddress();
                msg = new String(inPacket.getData(), 0, inPacket.getLength());
                System.out.println("Client: " + source_address + ":" + source_port);

                // Pull details about available files.
                String dirname = System.getProperty("user.dir");
                File path = new File(dirname);
                File fileList[] = path.listFiles();

                // String to hold names of all files/info about files.
                StringBuilder fileNames = new StringBuilder("\n");
                
                // Find the number of files available.
                int count = 0;
                for (int i = 0; i < fileList.length; i++){

                    if (fileList[i].canRead())
                        count++;
                }
                fileNames.append(count + " files found.\n\n");

                // Find the details for available files.
                for (int i = 0; i < fileList.length; i++){

                    fileNames.append(fileList[i].getName() + " " + fileList[i].length() + " Bytes\n");
                }
                
                // Prompt the client for input.
                fileNames.append("\nEnter file name for download: ");

                // Send all information to the client(includes prompt).
                outBuf = (fileNames.toString()).getBytes();
                outPacket = new DatagramPacket(outBuf, 0, outBuf.length, source_address, source_port);
                socket.send(outPacket);

                // Retrieve clients file selection, store file name.
                inBuf = new byte[100];
                inPacket = new DatagramPacket(inBuf, inBuf.length);
                socket.receive(inPacket);
                String fileName = new String(inPacket.getData(), 0, inPacket.getLength());
                System.out.println("Requested File " + fileName);
                
                // Check for the client request, if found set index location.
                boolean fileFound = false;
                int index = 1;
                for (int i = 0; i < fileList.length; i++){

                    if (((fileList[i].getName()).toString()).equalsIgnoreCase(fileName)){
                        index = i;
                        fileFound = true;
                    }
                }
                
                // If file wasn't found alert the client.
                if (!fileFound){
                    String noFile = "ERROR: file not found";
                    System.out.println(noFile);
                    outBuf = noFile.toString().getBytes();
                    outPacket = new DatagramPacket(outBuf, 0, outBuf.length, source_address, source_port);
                    socket.send(outPacket);
                }

                // The file was found.
                else{
                  
                    // Begin attempt to send file.
                    try{

                        // Open file and input stream.
                        File file = new File(fileList[index].getAbsolutePath());
                        InputStream fis = new FileInputStream(file);

                        // The buffer to read file to.
                        byte[] buffer = new byte[1020];
                        
                        // Continuously read from file until EOF.
                        int packetNum = 0;
                        int bytesRead;
                        index = 0;
                        boolean finished = false;
                        boolean exit = false;
                        int counter = 0;
						int windowStartIndex = 0; //0-10
                        int[] window = new int[5];
                        byte[][] storage = new byte[5][];
                        while((bytesRead = fis.read(buffer)) != -1){
                            exitLoop:
                            while(!exit){

                                if(packetNum > 740)
                                    System.out.println(index);

                                if(index < 5 && !finished){
                                
                                    // Display packet information.
                                    packetNum++;
                                
                                    // If this is the last packet to send.
                                    if(bytesRead < 1020){
                                        System.out.println("Last Packet");
                                        System.out.println(index);

                                        // Calculate header information.
                                        String stringSize = Integer.toString(bytesRead);
                                        int stringLength = stringSize.length();
                                        int[] a = new int[3];
                                        for(int i = 2, j = 0; i >= 0; i--, j++){
                                            if((stringLength-1)-j >= 0)
                                                a[i] = Integer.parseInt(stringSize.substring((stringLength-1)-j, stringLength-j));
                                            else
                                                a[i] = 0;
                                        }                      
                                        header = new byte[]{ (byte)11, (byte)a[0], (byte)a[1], (byte)a[2]};
                                        finished = true;
                                    }

                                    // This is not the last packet to send.
                                    else{
                                        header = new byte[]{ (byte)((windowStartIndex + index) % 10), (byte)0, (byte)0, (byte)0};
                                    }
                                   
                                    // Combine the header with the data.
                                    outBuf = new byte[header.length + buffer.length];
                                    System.arraycopy(header, 0, outBuf, 0, header.length);
                                    System.arraycopy(buffer, 0, outBuf, header.length, buffer.length);

                                    // Put the packet in storage.
                                    storage[index++] = outBuf;
                                    if (!finished)
                                        break;
                                }

								for(int i = 0; i < 5; i++){
									if(finished)
										System.out.println("Counter: " + i + " Index: " + index);
									if(i >= index){
										exit = true;
										break exitLoop;
									}
									System.out.printf("Sending Packet: %4d, Size: %d%n", (packetNum-index)+i+1, bytesRead);
                                    // Send the packet to the client.
                                    outBuf = new byte[1024];
                                    outBuf = storage[i];
                                    outPacket = new DatagramPacket(outBuf, 0, outBuf.length, source_address, source_port);
                                    socket.send(outPacket);
								}
                                System.out.println();

    // Make this timeout if no message recieved in some time instead of loop.
                                String inS;
                                int inI; 
								//clear the window
                                for(int x = 0; x < 5; x++)
                                    window[x] = 0;
								
								socket.setSoTimeout(500);
                                for(int i = 0; i < 5; i++){
									try{
										inBuf = new byte[100];
										inPacket = new DatagramPacket(inBuf, inBuf.length);
										socket.receive(inPacket);
										inS = new String(inPacket.getData(), 0, inPacket.getLength());
										inI = Integer.parseInt(inS);
										window[inI] = 1;
									}catch (SocketTimeoutException ex){
										break;
									}finally{
										socket.setSoTimeout(0);
									}
                                }

								//calculate slide
                                int slideIndex = 5;
                                for(int i = 0; i < 5; i++){
                                    if(window[i] == 0){
                                        slideIndex = i;
                                        break;
                                    }
                                }
								
								//slide storage
                                boolean max = false;
                                boolean checked = false;
                                for(int i = 0; i < 5; i++){
                                    if(!checked && i + slideIndex >= 5){
                                        max = true;
                                        checked = true;
                                        slideIndex = i;
                                    }
                                    if(max)
                                        storage[i] = null;
                                    else{
                                        storage[i] = storage[i + slideIndex];
                                    } 
                                }
								
								index = slideIndex;
								windowStartIndex = (windowStartIndex += slideIndex % 10);
                            }
                        }   
                        
                        // Check that file reading was finished.
                        if(fis.read() == -1){
                            System.out.println("File Read Successful. Closing Socket.");
                        } 
                        fis.close(); 
						System.exit(0);
                    }
                    catch(IOException ioe){

                        System.out.println(ioe);
                    }
                }
            }
        }
        catch(Exception e){

            System.out.println("Error\n");
            e.printStackTrace();
        }
    }
}