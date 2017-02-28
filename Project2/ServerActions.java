//package project2;

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


/*************************************************************************
* Class: ServerActions
* 
*  The ServerActions class contains all basic methods that the server
*  would need in order to carry out its operations. Can be considered a 
*  helper class to held declutter the UDPserver class.  
*************************************************************************/
public class ServerActions {
	
	private File[] fList;
	private File file;
	private InputStream fis;
	private byte[] buffer;
	private int bytesRead;
	

	/*********************************************************************
	* Method: fileList
	* 
	* Finds the names and sizes of all the files within the directory in 
	* which the program was executed. Creates a string to hold all of the
	* file information, returns the total number of files found, the names
	* of all files, and the size of all files.
	* 
	* @param  (void)
	* @return (void)
	*********************************************************************/
	public String fileList(){
		String dirname = System.getProperty("user.dir");
		File directory  = new File(dirname);
		fList = directory.listFiles();
		StringBuilder fileNames = new StringBuilder("\n");
		
		int totalFiles = 0;
		for (File file : fList){
			if (file.isFile()){
				totalFiles++;
				fileNames.append(file.getName() + " ----- " + file.length() + " Bytes\n");
			}
		}
		fileNames.insert(0, "\n" + totalFiles + " files found\n\n");
		fileNames.append("\nEnter a file name for transfer: ");
		return fileNames.toString();
	}
	
	/*********************************************************************
	* Method: fileFound
	* 
	* Determines if the parameter passed filename is a valid file within
	* the programs directory.
	* 
	* @param  (String fileName) -The name of the file to check.
	* @return (boolean true)    -If the fileName was a valid file.
	* @return (boolean false)   -If the fileName was not a valid file.
	*********************************************************************/
	public boolean fileFound(String fileName){
		if(fList == null)
			return false;
		
		for (File file : fList){
			if ((file.getName()).equals(fileName))
				return true;
		}
		return false;
	}
	
	/*********************************************************************
	* Method: getPort
	* 
	* (I/O) Asks the user to enter a port number. Reads the console line 
	* and error checks that the port was valid.
	* 
	* @param  (void)
	* @return (int port) -The port the server will use for connections.
	*********************************************************************/
	public int getPort(){
		Console cons = System.console();
		int port = Integer.parseInt(cons.readLine("Please enter a port number: "));
		if(port < 1024 || port > 65535)
			try {
				throw new IOException("This port is not valid");
			} catch (IOException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
		return port;
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
	public DatagramPacket getClientMsg(DatagramSocket socket){
		byte[] inBuf = new byte[100];
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
	* Method: sendMsg
	* 
	* Sends the message(msg) to the client.
	* 
	* @param (String msg)            -Message to the client.
	* @param (DatagramSocket socket) -Socket connection.
	* @param (InetAddress address)   -The clients IP address.
	* @param (int port)              -The clients port.
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
	public void sendData(byte[] msg, DatagramSocket socket, InetAddress address, int port){
		byte[] outBuf = msg;
        DatagramPacket outPacket = new DatagramPacket(outBuf, 0, outBuf.length, address, port);
        try {
			socket.send(outPacket);
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
	}
	
	//send a whole window
	public void sendWindow(SlidingWindow window, DatagramSocket socket, InetAddress address, int port){
		for(SlidingPacket packet : window.packets()){
			if(!packet.acknowledged()){
				byte[] checksum = this.createChecksum(packet.getPacket(false, null));
				byte[] toSend = packet.getPacket(true, checksum);
				DatagramPacket outPacket = new DatagramPacket(toSend, 0, packet.length(), address, port);
				try {
					socket.send(outPacket);
				} catch (IOException e) {
					// TODO Auto-generated catch block
					e.printStackTrace();
				}
			}
		}
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
	
	/*********************************************************************
	* Method: setupFileTransfer
	* 
	* Set the file to read from, create the file input stream, and create
	* the buffer to store the data in.
	* 
	* @param  (String fileName) -The file to read the data of.
	* @return (void)
	*********************************************************************/
	public void setupFileTransfer(String fileName){
		file = new File(fileName);
		try {
			fis = new FileInputStream(file);
		} catch (FileNotFoundException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		buffer = new byte[1020];
	}
	
	/*********************************************************************
	* Method: readData
	* 
	* Read the next packet sized amount of the file. Store the data in buffer.
	* 
	* @param  (void)
	* @return (int bytesRead) -The amount of the file that was read.
	*********************************************************************/
	public int readData(){
		try {
			bytesRead = fis.read(buffer);
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		return bytesRead;
	}
	
	
	/*********************************************************************
	* Method: getData
	* 
	* Returns the data in buffer(buffer being a fragment of the file
	* being transfered).
	* 
	* @param  (void)
	* @return (byte[] buffer) -Data consisting of a fragment of a file.
	*********************************************************************/
	public byte[] getData(){
		return buffer;
	}
	
	/*********************************************************************
	* Method: getBytesRead
	* 
	* Returns the last number of bytes read from the file being transfered.
	* 
	* @param  (void)
	* @return (int bytesTransfered) -Number of bytes last read.
	*********************************************************************/
	public int getBytesRead(){
		return bytesRead;
	}
	
	public byte[] createChecksum(byte[] packet){
		
		int total = 0;
		
		for (int i = 0; i < packet.length; i++){
			total += (packet[i] & 0xFF);
		}
		
		byte[] checksum = new byte[] { (byte)(total >> 24),
									   (byte)(total >> 16),
									   (byte)(total >> 8),
								       (byte)(total)};
		
		return checksum;
	}
	
	
	
	/*********************************************************************
	* Method: getSeqNum
	* 
	* Returns sequence number that the client has sent an acknowledgement for,
	* will return -1 if there was corruption.
	* 
	* @param  (DatagramPacket packet)
	* @return (int -1)    -Negative One for there was corruption.
	* @return (int index) -The seq num of the packet.
	*********************************************************************/
	public byte getSeqNum(DatagramPacket packet){
		byte index = 0;
		byte[] tempBuf = packet.getData();
		index = tempBuf[0];
		for (int i = 1; i < 4; i++){
			byte temp = tempBuf[i];
			if (temp != index){
				return -1;
			}
		}
		
		return index;
	}
	
	
}