//Jarod Hanko and Norman Cunningham
//3/3/2017

import java.io.Console;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.SocketException;

import java.io.*;
import java.net.*;
import java.nio.*;
import java.nio.channels.*;
import java.util.*;

public class SharedActions {
	public byte[] signPacket(byte[] packet, byte number) throws IllegalArgumentException{
		if(number == 0xF){
			throw new IllegalArgumentException("0xF is reserved for termination packets");
		}
		byte[] result = new byte[packet.length + 4];
		result[0] = 0xF;
		result[1] = 0xF;
		result[2] = 0xF;
		result[3] = number;
		System.arraycopy(packet, 0, result, 4, packet.length);
		return result;
	}
	
	public byte getPacketSign(byte[] signedPacket) throws IllegalArgumentException{
		if(signedPacket[0] != 0xF || signedPacket[1] != 0xF || signedPacket[2] != 0xF){
			throw new IllegalArgumentException("This packet is not signed");
		}
		return signedPacket[3];
	}
	
	public byte[] designPacket(byte[] signedPacket){	
		byte[] result = new byte[signedPacket.length - 4];
		System.arraycopy(signedPacket, 4, result, 0, signedPacket.length - 4);
		return result;
	}
	
	public byte[] signedAck(byte number){
		return signPacket(new byte[0], number);
	}
	
	public byte getAckNumber(byte[] ack) throws IllegalArgumentException{
		if(ack.length > 4){
			throw new IllegalArgumentException("Not an ack");
		}
		return this.getPacketSign(ack);
	}
	
	
	/*********************************************************************
	* Method: sendData
	* 
	* Send the packet to the client.
	* 
	* @param (byte[] msg)            -The packet being sent
	* @param (DatagramSocket socket) -Socket connection.
	* @param (InetAddress address)   -The clients IP address.
	* @param (int port)              -The clients port.
	* @return (void)
	*********************************************************************/
	public void sendData(byte[] msg, DatagramSocket socket, InetAddress address, int port) {
		byte[] outBuf = msg;
        DatagramPacket outPacket = new DatagramPacket(outBuf, 0, outBuf.length, address, port);
        try {
			socket.send(outPacket);
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
	}
	
	
	/*********************************************************************
	* Method: getClientMsg
	* 
	* Waits for a message from a client and returns the datagram packet
	* that it receives from the client.
	*  
	* @param  (DatagramSocket socket)   -Socket connection.
	* @return (DatagramPacket inPacket) -The packet received from the client.
	*********************************************************************/
	public DatagramPacket getMsg(DatagramSocket socket) throws SocketTimeoutException{
		byte[] inBuf = new byte[1024];
		DatagramPacket inPacket = new DatagramPacket(inBuf, inBuf.length);
		try {
			socket.receive(inPacket);
		}catch (SocketTimeoutException ex){
			throw new SocketTimeoutException("Scary Exception");
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
        return inPacket;
	}
	
	public String byteArrayToString(byte[] inPacket){
		String inString = new String(inPacket, 0, inPacket.length);
		return inString;
	}
}