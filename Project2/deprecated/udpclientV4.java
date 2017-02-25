/*************************************************************************
* Author:    Norman Cunningham, Jarod Hanko
* File:      udpclient 
* Version:   4.17
* Task:      Project 2A
* Date Due:  02/21/2017 
*************************************************************************/

import java.io.*;
import java.net.*;
import java.util.Scanner;
import java.util.Arrays;

class udpclient{

    public static void main(String args[]){

        DatagramSocket socket = null;
        DatagramPacket inPacket = null;
        DatagramPacket outPacket = null;
        byte[] inBuf;
        byte[] outBuf;
        final int PORT = 50000;
        String msg = null;
        Scanner src = new Scanner(System.in);

        try{

            InetAddress address = InetAddress.getByName("127.0.0.1");
            socket = new DatagramSocket();

            // Send signal to server.
            msg = "";
            outBuf = msg.getBytes();
            outPacket = new DatagramPacket(outBuf, 0, outBuf.length, address, PORT);
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
            outPacket = new DatagramPacket(outBuf, 0, outBuf.length, address, PORT);
            socket.send(outPacket);

            // Begin recieve file process.
            data = "";
            byte[] buffer = new byte[65535];
            OutputStream fos = new FileOutputStream("~"+filename);
            
            // Continue to look for packets until finished.
            while(true){

                // Grab the next packet.
                inBuf = new byte[1024];
                inPacket = new DatagramPacket(inBuf, inBuf.length);
                socket.receive(inPacket);

                // Check header to see if last packet. If so, determine how much data
                // was sent, and resize the buffer accordingly. Write buffer to file.
                if(inBuf[0] == (byte)1){
                    String sSize = "";
                    for(int i = 0; i < 3; i++){
                        sSize += Integer.toString((int)inBuf[i+1]);
                    }
                    int iSize = Integer.parseInt(sSize);
                    byte[] tempBuf = Arrays.copyOfRange(inBuf, 4, 4+iSize);
                    fos.write(tempBuf);
                    break;
                }

                // Was not the last packet, write to file, excluding the header.
                byte[] tempBuf = Arrays.copyOfRange(inBuf, 4, inBuf.length);
                fos.write(tempBuf);

            }

            // Close output stream.
            fos.close();
        }
        catch(Exception e)
        {
            System.out.println("\nNetwork Error. Please try again.\n");
            e.printStackTrace();
            System.out.println(e.getMessage());
        }
    }
}