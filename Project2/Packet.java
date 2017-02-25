//package project2;


/*************************************************************************
* Class: Packet
* 
*  The Packet class contains the overall complete packet, the header section,
*  the data section, an acknowledgement flag, a sequence number, and the data
*  length. 
*************************************************************************/
public class Packet {
	
	private byte[] packet;
	private byte[] header;
	private byte[] data;
	private boolean acknowledged;
	private byte number;
	private int length;
	private int seqNum = 0;
	private final int headerLength = 4;
	
	/*********************************************************************
	* Method: newPacket
	* 
	* Constructs a new packet. Calculates the header given the parameters
	* 
	* @param  (byte[] newData) -The data of the new packet.
	* @return (int dataLength) -The length of the data.
	*********************************************************************/
	public void newPacket(byte[] newData, int dataLength){
		acknowledged = false;
		number = (byte) seqNum++;
		if(seqNum >= 10)
			seqNum = 0;
		length = dataLength;
		
		header = new byte[]{number, (byte)((length >>> 8) & 0xFF), (byte)(length & 0xFF), 0};
		data = newData;
		packet = new byte[headerLength + dataLength];
		
		System.arraycopy(header, 0, packet, 0, header.length);
		System.arraycopy(data, 0, packet, header.length, data.length);
	}
	
	/*********************************************************************
	* Method: getData
	* 
	* Returns the data of the packet.
	* 
	* @param  (void)
	* @return (byte[] data) -The data of the packet.
	*********************************************************************/
	public byte[] getData(){
		return data;
	}
	
	/*********************************************************************
	* Method: getPacket
	* 
	* Returns the entire packet array, being header and data in one array.
	* 
	* @param  (void)
	* @return (byte[] packet) -The header and data of the packet.
	*********************************************************************/
	public byte[] getPacket(){
		return packet;
	}
	
	/*********************************************************************
	* Method: getAcknowledged
	* 
	* Get the acknowledgement status of the packet. 
	* 
	* @param  (void)
	* @return (boolean acknowledged) -True if the packet has been acknowledged.
	*********************************************************************/
	public boolean getAcknowledged(){
		return acknowledged;
	}
	
	/*********************************************************************
	* Method: getNumber
	* 
	* Get the sequence number of the packet.
	* 
	* @param  (void)
	* @return (int number) -Cast the byte to an integer.
	*********************************************************************/
	public int getNumber(){
		return (number & 0xFF);
	}
	
	/*********************************************************************
	* Method: getLength
	* 
	* Get the length of the data in the packet.
	* 
	* @param  (void)
	* @return (int length) -Cast the byte to an integer.
	*********************************************************************/
	public int getLength(){
		return (length & 0xFF);
	}
	
	/*********************************************************************
	* Method: setAcknowledged
	* 
	* Set that the packet has been acknowledged.
	* 
	* @param  (void)
	* @return (void)
	*********************************************************************/
	public void setAcknowledged(){
		acknowledged = true;
	}

}
