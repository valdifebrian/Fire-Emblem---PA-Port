// Ghidra headless postScript: dump functions + strings to TSV files given as script args.
// Usage: -postScript DumpFns.java <functions.tsv> <strings.tsv>
import ghidra.app.script.GhidraScript;
import ghidra.program.model.data.StringDataType;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Function;
import java.io.PrintWriter;

public class DumpFns extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        String fnPath = args.length > 0 ? args[0] : "functions.tsv";
        String stPath = args.length > 1 ? args[1] : "strings.tsv";

        try (PrintWriter pw = new PrintWriter(fnPath)) {
            pw.println("addr\tname\tparams\tbody_bytes\tthunk");
            int n = 0;
            for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                if (monitor.isCancelled()) break;
                long sz = f.getBody().getNumAddresses();
                pw.printf("%08x\t%s\t%d\t%d\t%s%n",
                    f.getEntryPoint().getUnsignedOffset(), f.getName(),
                    f.getParameterCount(), sz, f.isThunk());
                n++;
            }
            println("DumpFns: functions=" + n + " -> " + fnPath);
        }

        try (PrintWriter pw = new PrintWriter(stPath)) {
            pw.println("addr\tlen\ttext");
            int n = 0;
            for (Data d : currentProgram.getListing().getDefinedData(true)) {
                if (monitor.isCancelled()) break;
                if (d.getDataType() instanceof StringDataType) {
                    Object v = d.getValue();
                    if (v instanceof String s && s.length() >= 4) {
                        pw.printf("%08x\t%d\t%s%n", d.getMinAddress().getUnsignedOffset(), s.length(),
                            s.replace("\t", " ").replace("\n", "\\n"));
                        n++;
                    }
                }
            }
            println("DumpFns: strings=" + n + " -> " + stPath);
        }
    }
}
