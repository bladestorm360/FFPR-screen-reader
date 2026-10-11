// Read-only batch export. Args: output.c manifest.tsv (VA<TAB>label).
// Use an existing project with -process -noanalysis -readOnly. No analysis/decompile timeout.
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import java.nio.file.*;
import java.io.*;

public class DecompileManifest extends GhidraScript {
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        try (PrintWriter out = new PrintWriter(args[0], "UTF-8")) {
            for (String line : Files.readAllLines(Paths.get(args[1]))) {
                if (line.trim().isEmpty() || line.startsWith("#")) continue;
                String[] row = line.split("\t", 2);
                Address addr = toAddr(Long.parseLong(row[0].replace("0x", ""), 16));
                Function fn = getFunctionContaining(addr);
                out.println("/* ==== " + line + " ==== */");
                if (fn == null) {
                    // Matching metadata can identify an unanalyzed function. Changes are
                    // discarded by the required -readOnly analysis session.
                    disassemble(addr);
                    fn = createFunction(addr, row.length > 1 ? row[1].replace('.', '_') : null);
                }
                if (fn == null) { out.println("/* NO FUNCTION */"); continue; }
                DecompileResults r = di.decompileFunction(fn, 0, monitor);
                out.println(r.getDecompiledFunction() == null ? "/* " + r.getErrorMessage() + " */" : r.getDecompiledFunction().getC());
                out.flush();
            }
        } finally { di.dispose(); }
    }
}
