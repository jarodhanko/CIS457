import java.io.*;
import java.net.*;

class udpserver
{
   public static void main(String args[])
    {
        DatagramSocket socket = null;
        DatagramPacket inPacket = null;
        DatagramPacket outPacket = null;
        byte[] inBuf;
        byte[] outBuf;
        String msg;
        final int PORT = 50000;

        try
        {
        	socket = new DatagramSocket(PORT);

        	while(true)
        	{
        		System.out.println("\nRunning...\n");

        		inBuf = new byte[100];
        		inPacket = new DatagramPacket(inBuf, inBuf.length);
        		socket.receive(inPacket);

        		int source_port = inPacket.getPort();
        		InetAddress source_address = inPacket.getAddress();
        		msg = new String(inPacket.getData(), 0, inPacket.getLength());
        		System.out.println("Client: " + source_address + ":" + source_port);

        		String dirname = "/home/cunningn/457";
        		File f1 = new File(dirname);
        		File fl[] = f1.listFiles();

        		StringBuilder sb = new StringBuilder("\n");
        		int c = 0;

        		for (int i = 0; i < fl.length; i++)
        		{
        			if (fl[i].canRead())
        				c++;
        		}

        		sb.append(c + " files found.\n\n");

        		for (int i = 0; i < fl.length; i++)
        		{
        			sb.append(fl[i].getName()+" "+fl[i].length()+" Bytes\n");
        		}
        		sb.append("\nEnter file name for download: ");
        		outBuf = (sb.toString()).getBytes();
        		outPacket = new DatagramPacket(outBuf, 0, outBuf.length, source_address, source_port);
        		socket.send(outPacket);

        		inBuf = new byte[100];
        		inPacket = new DatagramPacket(inBuf, inBuf.length);
        		socket.receive(inPacket);
        		String filename = new String(inPacket.getData(), 0, inPacket.getLength());

        		System.out.println("Requested File "+filename);
        		
        		boolean flis = false;
        		int index = 1;
        		sb = new StringBuilder("");
        		for (int i = 0; i < fl.length; i++)
        		{
        			if (((fl[i].getName()).toString()).equalsIgnoreCase(filename)){
        				index = i;
        				flis = true;
        			}
        		}
        		if (!flis){
        			System.out.println("ERROR");
        			sb.append("ERROR");
        			outBuf = (sb.toString()).getBytes();
        			outPacket = new DatagramPacket(outBuf, 0, outBuf.length, source_address, source_port);
        			socket.send(outPacket);
        		}
        		else
        		{
        			try
        			{
        				// File Send Process, Independent
        				File ff = new File(fl[index].getAbsolutePath());
    
        				String s = null;
        				sb = new StringBuilder();


                        InputStream fis = new FileInputStream(ff);

                        byte[] buffer = new byte[1024];
                        int i = 0;
                        int j = 5;
                        int read;
                        System.out.println("STR-BlD:" + sb.toString()+":");
        				while((read = fis.read(buffer)) != -1)
        				{

                            i++;
                            s = new String(buffer);
                            sb.append(s);
                            System.out.println(read);
                            if (read < 1021){
                                s = "END"+s;
                                sb.append("END");
                            }
                            System.out.println(s);
                           
                                i = 0;
                                
                                System.out.println("HELLOs");
                                //sb.append(s);
                                outBuf = new byte[1024];
                                outBuf = s.getBytes();
                                outPacket = new DatagramPacket(outBuf, 0, outBuf.length, source_address, source_port);
                                socket.send(outPacket);
                                //s = null;
                                sb = new StringBuilder();
                                           
        				}
                        
        				
        				if(fis.read()==-1){
        					System.out.println("File Read Successful. Closing Socket.");
        				}

    
        			}
        			catch(IOException ioe)
        			{
        				System.out.println(ioe);
        			}
        		}

        	}
        }
        catch(Exception e)
        {
        	System.out.println("Error\n");
        	e.printStackTrace();
        }
    }
}