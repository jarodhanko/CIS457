import java.io.*;
import java.net.*;
import java.util.Scanner;

class udpclient
{
	public static void main(String args[])
	{
   		DatagramSocket socket = null;
        DatagramPacket inPacket = null;
        DatagramPacket outPacket = null;
        byte[] inBuf;
        byte[] outBuf;
        final int PORT = 50000;
        String msg = null;
        Scanner src = new Scanner(System.in);

        try
        {
        	//String addr = src.nextLine();
        	InetAddress address = InetAddress.getByName("127.0.0.1");
        	socket = new DatagramSocket();

        	msg = "";
        	outBuf = msg.getBytes();
        	outPacket = new DatagramPacket(outBuf, 0, outBuf.length, address, PORT);
        	socket.send(outPacket);

        	inBuf = new byte[1024];
        	inPacket = new DatagramPacket(inBuf, inBuf.length);
        	socket.receive(inPacket);

        	String data = new String(inPacket.getData(), 0, inPacket.getLength());
        	// Print file list.
        	System.out.println(data);

        	// Send file name.
        	String filename = src.nextLine();
        	outBuf = filename.getBytes();
        	outPacket = new DatagramPacket(outBuf, 0, outBuf.length, address, PORT);
        	socket.send(outPacket);

            // Recieve file.
            //FileOutputStream  fos = new FileOutputStream("~" + filename);
            data = "";
            byte[] buffer = new byte[65535];
            StringBuilder sb = new StringBuilder();
            int z = 0;
            while(true){
                z = 0;
                System.out.println("HELLO");
                inBuf = new byte[1024];
                inPacket = new DatagramPacket(inBuf, inBuf.length);
                socket.receive(inPacket);
                //byte[] bytes = inPacket.getData();
                data = new String(inPacket.getData(), 0, inPacket.getLength());
                data = data.trim();
                //data = data.replaceAll("\\u00A0", null);
                System.out.println(data);
                if (data.endsWith("ERROR")) 
                {
                    System.out.println("ERROR IN");
                    break; 
                }
                if(data.startsWith("END"))
                {   
                    System.out.println("END");
                    System.out.println(data);
                    data = data.substring(3, data.length());
                    sb.append(data);
                    z = 1;
                }
                if (z == 1) 
                {
                    System.out.println("BREAK");
                    break;
                }
                sb.append(data);
                

            }
             String temp = sb.toString();
            System.out.println(temp);
             byte[] b = temp.getBytes();
             System.out.println();
             int j = sb.toString().length();
             System.out.println(j);
             //fos.write(b);
             
            //fos.close();
        
        	if (data.endsWith("ERROR")) 
        	{
        		System.out.println("File doesn't exist.\n");
        		socket.close();	
        	}
        	else
        	{
        		//try
        		//{
        			BufferedWriter pw = new BufferedWriter(new OutputStreamWriter(new FileOutputStream("~"+filename)));
        			pw.write(sb.toString());
        			// Force write buffer to file
        			pw.close();

        			System.out.println("File Write Successful. Closing Socket.");
        			socket.close();
        		//}
        		//catch(IOException ioe)
        		//{
        			//System.out.println("File Error\n");
        			socket.close();
        		//}
        	}
        }
        catch(Exception e)
        {
        	System.out.println("\nNetwork Error. Please try again.\n");
        }
    }
}