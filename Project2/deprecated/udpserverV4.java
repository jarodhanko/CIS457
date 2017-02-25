/*************************************************************************
* Author:    Norman Cunningham, Jarod Hanko
* File:      udpserver 
* Version:   4.17
* Task:      Project 2A
* Date Due:  02/21/2017 
*************************************************************************/

import java.io.*;
import java.net.*;

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
                int c = 0;
                for (int i = 0; i < fileList.length; i++){

                    if (fileList[i].canRead())
                        c++;
                }
                fileNames.append(c + " files found.\n\n");

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
                        while((bytesRead = fis.read(buffer)) != -1){
                            
                            // Display packet information.
                            packetNum++;
                            System.out.printf("Packet: %4d, Size: %d%n", packetNum, bytesRead);
                            
                            // If this is the last packet to send.
                            if(bytesRead < 1020){

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
                                header = new byte[]{ (byte)1, (byte)a[0], (byte)a[1], (byte)a[2]};
                            }

                            // This is not the last packet to send.
                            else{
                                header = new byte[]{ (byte)0, (byte)0, (byte)0, (byte)0};
                            }
                               
                            // Combine the header with the data.
                            outBuf = new byte[header.length + buffer.length];
                            System.arraycopy(header, 0, outBuf, 0, header.length);
                            System.arraycopy(buffer, 0, outBuf, header.length, buffer.length);

                            // Send the packet to the client.
                            outPacket = new DatagramPacket(outBuf, 0, outBuf.length, source_address, source_port);
                            socket.send(outPacket);                       
                        }   
                        
                        // Check that file reading was finished.
                        if(fis.read() == -1){
                            System.out.println("File Read Successful. Closing Socket.");
                        } 
                        fis.close(); 
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