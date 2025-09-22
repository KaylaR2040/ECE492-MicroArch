import java.io.*;

/* "args" holds the command-line arguments.

    Example:
    java sim 32 8192 4 262144 8 3 10 gcc_trace.txt
    args[0] = "32"
    args[1] = "8192"
    args[2] = "4"
    ... and so on
*/
public class sim {
    public static void main(String[] args) {

        cache_params params = new cache_params();	// Look at cache_params.java for the definition of class cache_params.
        String trace_file;				// This variable holds the trace file name
        char rw;					// This variable holds the request's type (read or write) obtained from the trace.
        long addr;					// This variable holds the request's address obtained from the trace.

        // Exit with an error if the number of command-line arguments is incorrect.
        if (args.length != 8) {
            System.out.println("Error: Expected 8 command-line arguments but was provided " + args.length);
            System.exit(0);
        }

        // Long.parseLong() converts string to long.
        params.BLOCKSIZE = Long.parseLong(args[0]);
        params.L1_SIZE   = Long.parseLong(args[1]);
        params.L1_ASSOC  = Long.parseLong(args[2]);
        params.L2_SIZE   = Long.parseLong(args[3]);
        params.L2_ASSOC  = Long.parseLong(args[4]);
        params.PREF_N    = Long.parseLong(args[5]);
        params.PREF_M    = Long.parseLong(args[6]);
        trace_file       = args[7];

        // Print simulator configuration.
        System.out.printf("===== Simulator configuration =====%n");
        System.out.printf("BLOCKSIZE:  %d%n", params.BLOCKSIZE);
        System.out.printf("L1_SIZE:    %d%n", params.L1_SIZE);
        System.out.printf("L1_ASSOC:   %d%n", params.L1_ASSOC);
        System.out.printf("L2_SIZE:    %d%n", params.L2_SIZE);
        System.out.printf("L2_ASSOC:   %d%n", params.L2_ASSOC);
        System.out.printf("PREF_N:     %d%n", params.PREF_N);
        System.out.printf("PREF_M:     %d%n", params.PREF_M);
        System.out.printf("trace_file: %s%n", trace_file);
        System.out.printf("%n");

        // Read requests from the trace file and echo them back.
        try (BufferedReader br = new BufferedReader(new FileReader(trace_file))) {
            String line;
            while ((line = br.readLine()) != null) {
                rw = line.charAt(0);				// Gets r/w char from String line.
                addr = Long.parseLong(line.substring(2), 16);	// Gets address from String line and converts to long. "parseLong" uses base 16.
                if (rw == 'r')
                    System.out.printf("r %x%n", addr);
                else if (rw == 'w')
                    System.out.printf("w %x%n", addr);
                else
                    System.err.format("Error: Unknown request type %c.%n", rw);

                ///////////////////////////////////////////////////////
                // Issue the request to the L1 cache instance here.
                ///////////////////////////////////////////////////////
            }
        }
        catch (IOException x) {		// Throw error if file I/O fails.
            System.err.format("IOException: %s.%n", x);
        }
    }
}
