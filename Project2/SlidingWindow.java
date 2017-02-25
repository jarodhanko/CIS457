//package project2;

import java.util.LinkedList;
import java.util.*;

public class SlidingWindow{
	
	private LinkedList<SlidingPacket> packets;	
	
	public int slideIndex;
	public byte windowSize;
	public byte maxSize;
	
	public SlidingWindow(){
		slideIndex = 0;
		maxSize = 5;
		windowSize = (byte) (2 * maxSize);
		packets = new LinkedList<SlidingPacket>();
	}	
	
	public LinkedList<SlidingPacket> packets(){
		return this.packets;
	}
	
	public void addPacket(SlidingPacket packet){
		packets.add(packet);
		//sort packets by sequence number
		Collections.sort(packets, new Comparator<SlidingPacket>(){
			@Override
			public int compare(SlidingPacket pack1, SlidingPacket pack2){
				return pack1.seqNumber() - pack2.seqNumber();
			}
		});
	}

	public void slide(){
		@SuppressWarnings("unchecked")
		LinkedList<SlidingPacket> copy = (LinkedList<SlidingPacket>) packets.clone();
		for(int i = 0; i < packets.size(); i++){
			if(copy.peek().acknowledged()){
				copy.pop();
				slideIndex = (slideIndex++ % windowSize);
			}else{
				break;
			}
		}	
		this.packets = copy;
	}
	
	public void setAcknowledged(int index){
		packets.get(index).acknowledge(true);
	}
}
