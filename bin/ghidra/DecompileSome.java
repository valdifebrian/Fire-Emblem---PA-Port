// Ghidra headless postScript: decompile selected functions to .c files.
// Usage: -postScript DecompileSome.java <outDir> <hexAddr | s:exactString>...
// s:<str> finds defined string data with that exact value, then decompiles
// every function containing a reference TO it (string xref -> function).
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.StringDataType;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import java.io.File;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;

public class DecompileSome extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) { printerr("usage: DecompileSome.java <outDir> <addr|s:str>..."); return; }
        File dir = new File(args[0]);
        dir.mkdirs();
        DecompInterface decomp = new DecompInterface();
        decomp.openProgram(currentProgram);
        int done = 0;
        for (int i = 1; i < args.length && done < 12; i++) {
            String a = args[i];
            var funcs = new java.util.LinkedHashSet<Function>();
            if (a.startsWith("s:")) {
                String want = a.substring(2);
                for (Data d : currentProgram.getListing().getDefinedData(true)) {
                    if (!(d.getDataType() instanceof StringDataType)) continue;
                    if (!want.equals(d.getValue())) continue;
                    for (Reference ref : getReferencesTo(d.getMinAddress())) {
                        Function f = currentProgram.getFunctionManager().getFunctionContaining(ref.getFromAddress());
                        if (f != null) funcs.add(f);
                    }
                }
            } else {
                Address addr = currentProgram.getAddressFactory()
                    .getDefaultAddressSpace().getAddress(a.startsWith("0x") ? a.substring(2) : a);
                Function f = currentProgram.getFunctionManager().getFunctionContaining(addr);
                if (f != null) funcs.add(f);
            }
            for (Function f : funcs) {
                DecompileResults res = decomp.decompileFunction(f, 60, monitor);
                String body = res.decompileCompleted()
                    ? res.getDecompiledFunction().getC()
                    : "/* FAILED: " + res.getErrorMessage() + " */\n";
                File out = new File(dir, f.getName() + ".c");
                try (PrintWriter pw = new PrintWriter(out, StandardCharsets.UTF_8)) {
                    pw.println("// func " + f.getName() + " entry=" + f.getEntryPoint()
                        + " params=" + f.getParameterCount()
                        + (res.decompileCompleted() ? "" : " [DECOMPILE FAILED]"));
                    pw.print(body);
                }
                println("decompiled " + f.getName() + " (" + res.decompileCompleted() + ") -> " + out);
                done++;
            }
        }
        decomp.dispose();
        println("DecompileSome: wrote " + done + " functions");
    }
}
