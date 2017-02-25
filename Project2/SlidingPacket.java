//package project2;

import java.nio.ByteBuffer;

public class SlidingPacket {
	
	private byte[] data; //without header
	private boolean acknowledged;
	private byte seqNumber;
	private int number;
	private int length;
	private final int headerLength = 4;
	
	public SlidingPacket(){
		this.initialize(1024, null, false);
	}	
	
	public SlidingPacket(int length){
		this.initialize(length, null, false);
	}
	
	public void initialize(int length, byte[] data, boolean withHeader){		
		acknowledged = false;
		this.length = length;
		if(data != null){
			this.setData(data, withHeader);
		}
	}

	public void setData(byte[] data, boolean withHeader){
		if(!withHeader){
			this.data = data;
		}else{
			this.setFromPacket(data);
		}
	}
	
	public byte[] data(){
		return this.data;
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
	public byte[] getPacket(){
		byte[] header = this.createHeader();	
		byte[] tempBuf = new byte[header.length + data.length];		
		System.arraycopy(header, 0, tempBuf, 0, header.length);
		System.arraycopy(data, 0, tempBuf, header.length, data.length);
		return tempBuf;
	}
	
	//create header
	public byte[] createHeader(){
		int dataLength = data.length;
		int seqNum = this.seqNumber << 24; //bit shift 3 bytes since our number will be one byte max
		return ByteBuffer.allocate(4).putInt((int)(this.number + dataLength)).array(); //packetnum will be 0000 - 1010 shifted 12 bytes and datalength will be max 0000 0100 0000 0000
	}	
	
	public boolean setFromPacket(byte[] packet){
		int packetLength = packet.length;
		if(packetLength > this.length - this.headerLength)
			return false;
		
		this.seqNumber = packet[0];
		this.length = (packet[1] << 16) + (packet[2] << 8) + (packet[3] << 0); //binary shifting to add integer;
		this.data = new byte[this.length];
		System.arraycopy(packet, this.headerLength - 1, this.data, 0, this.length);
		return true;
	}
	
}
