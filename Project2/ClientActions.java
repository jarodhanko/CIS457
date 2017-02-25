//package project2;

import java.io.Console;
import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;


public class ClientActions {
	
	
	public String getIP(){
		Console cons = System.console();
		String addr = cons.readLine("Please enter an IP address: ");
		return addr;
	}
	
	public int getPort(){
		Console cons = System.console();
		int port = Integer.parseInt(cons.readLine("Please enter a port number: "));
		return port;
	}
	
	/*********************************************************************
	* Method: sendMsg
	* 
	* Sends the message(msg) to the server.
	* 
	* @param (String msg)            -Message to the server.
	* @param (DatagramSocket socket) -Socket connection.
	* @param (InetAddress address)   -IP address.
	* @param (int port)              -Port number.
	* @return (void)
	*********************************************************************/
	public void sendMsg(String msg, DatagramSocket socket, InetAddress address, int port){
		byte[] outBuf = msg.getBytes();
        DatagramPacket outPacket = new DatagramPacket(outBuf, 0, outBuf.length, address, port);
        try {
			socket.send(outPacket);
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
	}
	
	/*********************************************************************
	* Method: getServerMsg
	* 
	* Waits for a message from a Server and returns the datagram packet
	* that it receives from the server.
	*  
	* @param  (DatagramSocket socket)   -Socket connection.
	* @return (DatagramPacket inPacket) -The packet received from the server.
	*********************************************************************/
	public DatagramPacket getServerMsg(DatagramSocket socket){
		byte[] inBuf = new byte[1024];
		DatagramPacket inPacket = new DatagramPacket(inBuf, inBuf.length);
		try {
			socket.receive(inPacket);
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
        return inPacket;
	}
	
	/*********************************************************************
	* Method: packetToString
	* 
	* Converts a datagram packet into a string.
	* 
	* @param  (DatagramPacket inPacket) -The received message as a datagram.
	* @return (String inString)         -The received message as a string.
	*********************************************************************/
	public String packetToString(DatagramPacket inPacket){
		String inString = new String(inPacket.getData(), 0, inPacket.getLength());
		return inString;
	}

}
