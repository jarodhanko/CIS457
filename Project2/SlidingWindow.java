//package project2;

import java.util.LinkedList;

public class SlidingWindow{
	
	private LinkedList<SlidingPacket> packets;	
	
	public void slidingWindow(){
		slideIndex = 0;
		packets = new LinkedList<SlidingPacket>();
	}

	public int slideIndex;
	
	
	public LinkedList<SlidingPacket> packets(){
		return this.packets;
	}

	public void slide(){
		@SuppressWarnings("unchecked")
		LinkedList<SlidingPacket> copy = (LinkedList<SlidingPacket>) packets.clone();
		for(int i = 0; i < packets.size(); i++){
			if(packets.peek().acknowledged()){
				copy.pop();
			}else{
				break;
			}
		}	
		this.packets = copy;
	}
}
