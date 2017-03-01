//package project2;

import java.util.LinkedList;
import java.util.*;

public class SlidingWindow{
	
	private LinkedList<SlidingPacket> packets;	
	
	public byte slideIndex;
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
	
	public boolean addPacket(SlidingPacket packet){
		boolean result = false;
		byte upperbound = (slideIndex + maxSize) < windowSize ?  (byte)(slideIndex + maxSize) : (byte)windowSize;
		byte sn = packet.seqNumber();
		//if packet is within sliding window add it, else ignore it
		if((sn >= slideIndex && sn < upperbound) || 
			(((slideIndex + maxSize) >= windowSize) && 
			sn < ((slideIndex + maxSize) % windowSize))){
			//don't add duplicates
			for(SlidingPacket pk : packets){
				if(pk.seqNumber() == packet.seqNumber()){
					return true;
				}
			}
			packets.add(packet);
			result = true;
		}		
		//sort packets by sequence number
		Collections.sort(packets, new Comparator<SlidingPacket>(){
			@Override
			public int compare(SlidingPacket pack1, SlidingPacket pack2){
				//if the window is wrapping, smaller is bigger
				if ((pack1.seqNumber() < slideIndex && pack2.seqNumber() >= slideIndex)
					|| (pack2.seqNumber() < slideIndex && pack1.seqNumber() >= slideIndex)){
					return pack2.seqNumber() - pack1.seqNumber(); //-8,8,
				}
				return pack1.seqNumber() - pack2.seqNumber();//8,8,
			}
		});
		return result;
	}

	public boolean slide(){
		@SuppressWarnings("unchecked")
		LinkedList<SlidingPacket> copy = (LinkedList<SlidingPacket>) packets.clone();
		boolean changed = false;
		for(int i = 0; i < packets.size(); i++){
			if(copy.peek().acknowledged()){
				copy.pop();
				changed = true;
				System.out.println("SLIDEINDEX: " + this.slideIndex);
				this.slideIndex = (byte)((this.slideIndex + 1) % windowSize);
			}else{
				break;
			}
		}	
		
		this.packets = copy;
		return changed;
	}
	
	public void setAcknowledged(byte seqN){
		for(SlidingPacket p : this.packets){
			if(p.seqNumber() == seqN){
				p.acknowledge(true);
			}
		}		
	}
	
	public void setAcknowledged(int index){
		this.packets.get(index).acknowledge(true);
	}
	
	public void acknowledgeFirst(){
		this.packets.peek().acknowledge(true);
	}
	
	public boolean readyForSlide(){
		if(packets.peek() == null)
			return false;
		Collections.sort(packets, new Comparator<SlidingPacket>(){
			@Override
			public int compare(SlidingPacket pack1, SlidingPacket pack2){
				//if the window is wrapping, smaller is bigger
				if ((pack1.seqNumber() < slideIndex && pack2.seqNumber() >= slideIndex)
					|| (pack2.seqNumber() < slideIndex && pack1.seqNumber() >= slideIndex)){
					return pack2.seqNumber() - pack1.seqNumber(); //-8,8,
				}
				return pack1.seqNumber() - pack2.seqNumber();//8,8,
			}
		});
		int i = 0;
		for(SlidingPacket p : packets){
			System.out.println("Window [" + i + "] contains: " + p.seqNumber());
			i++;
		}
		System.out.println("ready to slide: " + packets.peek().seqNumber() + " slideIndex: " + slideIndex);
		if(packets.peek().seqNumber() == slideIndex)
			return true;
		return false;		
	}
}
