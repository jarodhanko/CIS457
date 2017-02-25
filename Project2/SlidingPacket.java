//package project2;

import java.nio.ByteBuffer;

public class SlidingPacket {
	
	private byte[] data; //without header
	private boolean acknowledged;
	private byte number;
	private int length;
	private final int headerLength = 4;
	
	public void initialize(int length, byte[] data, boolean withHeader){		
		acknowledged = false;
		this.length = length;
		if(data != null){
			this.setData(data, withHeader);
		}
	}
	public void slidingPacket(){
		this.initialize(1024, null, false);
	}	
	
	public void slidingPacket(int length){
		this.initialize(length, null, false);
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

	public boolean acknowledged(){
		return this.acknowledged;
	}
	
	public void acknowledge(boolean set){
		this.acknowledged = set;
	}
	
	public void setNumber(byte number){
		this.number = number;
	}
	
	public int number(){
		return this.number;
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
	public byte[] packet(byte[] data){
		byte[] header = this.createHeader();	
		byte[] tempBuf = new byte[header.length + data.length];		
		System.arraycopy(header, 0, tempBuf, 0, header.length);
		System.arraycopy(data, 0, tempBuf, header.length, data.length);
		return tempBuf;
	}
	
	//create header
	public byte[] createHeader(){
		int dataLength = data.length;
		int packetNum = this.number << 24; //bit shift 3 bytes since our number will be one byte max
		return ByteBuffer.allocate(4).putInt(packetNum + dataLength).array(); //packetnum will be 0000 - 1010 shifted 12 bytes and datalength will be max 0000 0100 0000 0000
	}	
	
	public void setFromPacket(byte[] packet){
		int packetLength = packet.length;
		if(packetLength > this.length - this.headerLength)
			try {
				throw new Exception("Mismatched Array Size");
			} catch (Exception e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
		
		this.number = packet[0];
		this.length = (packet[1] << 16) + (packet[2] << 8) + (packet[3] << 0); //binary shifting to add integer;
		this.data = new byte[this.length];
		System.arraycopy(packet, this.headerLength - 1, this.data, 0, this.length);
	}
	
}
