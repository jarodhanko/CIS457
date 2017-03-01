//package project2;

import java.nio.ByteBuffer;

public class SlidingPacket {
	
	private byte[] data; //without header
	private byte[] packet; // with header
	private boolean acknowledged;
	private byte seqNumber;
	private int number;
	private int length;
	private final int headerLength = 4;
	
	public SlidingPacket(){
		this.initialize(1024, null, false, false);
	}	
	
	public SlidingPacket(int length){
		this.initialize(length, null, false, false);
	}
	
	public void initialize(int length, byte[] data, boolean withHeader, boolean withChecksum){		
		acknowledged = false;
		this.length = length;
		if(data != null){
			this.setData(data, withHeader, withChecksum);
		}
	}

	public void setData(byte[] data, boolean withHeader, boolean withChecksum){
		if(!withHeader){
			this.data = data;
		}else{
			this.setFromPacket(data, withChecksum);
		}
	}
	
	public byte[] data(){
		return this.data;
	}
	
	public byte[] packet(){
		return packet;
	}
	
	public int length(){
		return this.length;
	}
	
	public boolean acknowledged(){
		return this.acknowledged;
	}
	
	public void acknowledge(boolean set){
		this.acknowledged = set;
	}
	
	public void setNumber(int number){
		this.number = number;
	}
	
	public int number(){
		return this.number;
	}

	public void setSequenceNumber(byte seqNumber){
		this.seqNumber = seqNumber;
	}
	
	public byte seqNumber(){
		return this.seqNumber;
	}
	
	public void clear(){
		this.acknowledged = false;
		this.data = null;
	}	
	
	public void clear(int length){
		this.acknowledged = false;
		this.data = null;
		this.length = length;
	}
	
	//assemble packet
	public byte[] getPacket(boolean addChecksum, byte[] checksum){
		
		if (!addChecksum){
			byte[] header = this.createHeader();	
			byte[] tempBuf = new byte[header.length + data.length];		
			System.arraycopy(header, 0, tempBuf, 0, header.length);
			System.arraycopy(data, 0, tempBuf, header.length, data.length);
			packet = tempBuf;
			return tempBuf;
		}
		else {
			byte[] header = checksum;
			byte[] packet = getPacket(false, null);
			byte[] tempBuf = new byte[header.length + packet.length];		
			System.arraycopy(header, 0, tempBuf, 0, header.length);
			System.arraycopy(packet, 0, tempBuf, header.length, packet.length);
			this.packet = tempBuf;
			return tempBuf;
		}
	}
	
	//create header
	public byte[] createHeader(){
		
		int dataLength = data.length;
		int seqNum = this.seqNumber << 24; //bit shift 3 bytes since our number will be one byte max
		return ByteBuffer.allocate(4).putInt((int)(seqNum + dataLength)).array(); //packetnum will be 0000 - 1010 shifted 12 bytes and datalength will be max 0000 0100 0000 0000
	}	
	
	public boolean setFromPacket(byte[] packet, boolean withChecksum){
		if(packet[0] == 0xF && packet[1] == 0xF && packet[2] == 0xF && packet[3] == 0xF)
			return false; //termination packet
		
		int checksumLength = withChecksum ? 4 : 0;
		int packetLength = packet.length - checksumLength;
		if(packetLength > this.length)
			return false;
		byte[] seq = {0,0,0,packet[0 + checksumLength]};
		this.seqNumber = (byte)ByteBuffer.allocate(4).put(seq).getInt(0);
		System.out.println("seqN: " + this.seqNumber);
		byte[] len = {0,packet[1 + checksumLength], packet[2 + checksumLength], packet[3 + checksumLength]};		
		this.length = ByteBuffer.allocate(4).put(len).getInt(0) + this.headerLength; //binary shifting to add integer;
		this.data = new byte[this.length - this.headerLength];
		System.arraycopy(packet, this.headerLength + checksumLength, this.data, 0, this.length - this.headerLength);
		return true;
	}
	
}
