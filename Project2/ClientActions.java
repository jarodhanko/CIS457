//package project2;

import java.io.Console;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.nio.ByteBuffer;


public class ClientActions {
	
	private OutputStream fos;
	
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
	
	public void sendMsg(byte[] outBuf, DatagramSocket socket, InetAddress address, int port){
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
	
	public boolean isLastPacket(DatagramPacket packet){
		
		byte[] tempBuf = packet.getData();
		if(tempBuf[0] == 0xF && tempBuf[1] == 0xF && tempBuf[2] == 0xF && tempBuf[3] == 0xF)
			return true; //termination packet
		//else check is data is shorter than max
		byte[] len = {0,tempBuf[5], tempBuf[6], tempBuf[7]};
		int dataSize = ByteBuffer.allocate(4).put(len).getInt(0);
		
		if(dataSize < 1016)
			return true;
		
		return false;
	}
	
	public void setupFile(String fileName){
		try {
			fos = new FileOutputStream("~"+fileName);
		} catch (FileNotFoundException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
	}
	
	public void closeFile(){
		try {
			fos.close();
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
	}
	
	public void writeToFile(byte[] data){
	
		try {
			fos.write(data);
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
	}
	
	public void sendAck(byte seqNum, DatagramSocket socket, InetAddress address, int port){
		byte[] tempBuf = new byte[] {seqNum, seqNum, seqNum, seqNum};  // Duplicated to be used as its own checksum.
		
		sendMsg(tempBuf, socket, address, port);
	}
	
	public boolean checkChecksum(byte[] packet){
		
		int total = 0;
		int checksum = ((0xFF & packet[0]) << 24) |
					   ((0xFF & packet[1]) << 16) |
					   ((0xFF & packet[2]) << 8) |
				       ((0xFF & packet[3]));
		
		for (int i = 4; i < packet.length; i++){
			total += (packet[i] & 0xFF);
		}
		
		System.out.println("\t \t \t \t CHECKSUM: " + checksum + " TOTAL: " + total);
		if (total == checksum)
			return true;
		
		return false;
	}
	
	public byte getSeqNum(DatagramPacket packet){
		return packet.getData()[4];
	}

}
