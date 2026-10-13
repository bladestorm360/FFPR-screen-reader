// Exact current-build targets in a disposable, no-analysis project. Never saves the program.
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.program.model.listing.Function;
import java.io.PrintWriter;
public class DecompileTargets extends GhidraScript {
    public void run() throws Exception {
        String[] args = getScriptArgs();
        try (PrintWriter out = new PrintWriter(args[0], "UTF-8")) {
            for (int i = 1; i < args.length; i++) {
                var address = toAddr(Long.parseUnsignedLong(args[i],16));
                disassemble(address);
                Function function = getFunctionAt(address);
                if (function == null) function = createFunction(address, "target_" + args[i]);
                var decompiler = new DecompInterface();
                decompiler.openProgram(currentProgram);
                var result = decompiler.decompileFunction(function,0,monitor);
                out.println("/* " + args[i] + " */");
                out.println(result.getDecompiledFunction() == null ? result.getErrorMessage() : result.getDecompiledFunction().getC());
                out.flush(); decompiler.dispose();
            }
        }
    }
}
