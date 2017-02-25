//package project2;

import java.util.LinkedList;

/*************************************************************************
* Class: Window
* 
*  The Window class, holds a linked list of packets that either a server
*  has sent and is waiting on acknowledgements from a client, or holds 
*  packets for a client who is reordering packets before they write to
*  a file.
*************************************************************************/
public class Window {
	
	private LinkedList<Packet> window;
	private int currentIndex;
	
	
	/*********************************************************************
	* Method: init
	* 
	* Initializes the window to a linked list of packets.
	* 
	* @param  (void)
	* @return (void)
	*********************************************************************/
	public void init(){
		window = new LinkedList<Packet>();
		currentIndex = 0;
	}
	
	/*********************************************************************
	* Method: addPacket
	* 
	* Adds a packet to the end of the window.
	* 
	* @param  (Packet packet) -The packet added to the window.
	* @return (void)
	*********************************************************************/
	public void addPacket(Packet packet){
		window.add(packet); 
	}
	
	/*********************************************************************
	* Method: slide
	* 
	* Starts at the beginning of the window. If the client received the packet,
	* and the server successfully received the acknowledgement, remove the 
	* packet from the window. Continue to do so until a non-acknowledged
	* packet is encountered. 
	* 
	* @param  (void)
	* @return (void)
	*********************************************************************/
	public void slide(){
		for (Packet packet : window){
			if (packet.getAcknowledged())
				window.pop();
			else
				break;
		}
	}
	
	/*********************************************************************
	* Method: setAcknowledged
	* 
	* Set the packet at the given index to acknowledged. 
	* 
	* @param  (int index) -The sequence number of the client acknowledgement packet
	* @return (void)
	*********************************************************************/
	public void setAcknowledged(int index){
		window.get(index).setAcknowledged();
	}
	

}
