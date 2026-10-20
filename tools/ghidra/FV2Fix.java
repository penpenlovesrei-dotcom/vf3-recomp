// Two corrections the match-flow region needs before it reads properly.
//
// 1. 0x0000ABF0 is the dispatcher's head, and every state handler ends by branching
//    BACK to it. It is in the middle of fv2_mode_tick, so the decompiler treats each
//    of those branches as something to inline and walks the whole chain of states --
//    a 208-byte handler came out as 4451 lines. Made a function of its own, the
//    branch is a tail call and each handler decompiles as itself.
//
// 2. Ghidra read the jump table at 0x00024354 as 212 entries. It is 80: the next
//    word, 0x00024494, is the first of the 80 name strings that run parallel to it,
//    and the four dispatchers index both at the same base (0, 24, 44, 50). Past 80 the
//    "destinations" are string pointers, which is why the decompiler reported bad
//    instruction data at 0x0008CE40 -- it was disassembling the name pool. This drops
//    every computed-jump reference outside each dispatcher's real span.
//
// @category FV2
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.CodeUnit;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FlowOverride;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.mem.MemoryAccessException;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceManager;
import ghidra.program.model.symbol.SourceType;

import java.io.BufferedReader;
import java.io.FileReader;
import java.util.HashSet;
import java.util.Set;

public class FV2Fix extends GhidraScript {

	// bctr address, jump-table address, how many entries that dispatcher may reach
	private static final long[][] DISPATCH = {
			{0x0000AC60L, 0x00024354L, 24},   // the main mode machine
			{0x0000BCC8L, 0x000243B4L, 20},   // GAME_DSP's sub modes
			{0x0000BAD4L, 0x00024404L, 6},    // SEL_DSP's sub modes
			{0x0000B138L, 0x0002441CL, 12},   // ADV_DSP's sub modes
	};

	@Override
	public void run() throws Exception {
		String[] args = getScriptArgs();
		Address head = toAddr(0x0000ABF0L);
		Function f = getFunctionAt(head);
		if (f == null) {
			new CreateFunctionCmd(head).applyTo(currentProgram, monitor);
			f = getFunctionAt(head);
		}
		if (f != null) {
			f.setName("fv2_mode_dispatch", SourceType.USER_DEFINED);
			currentProgram.getListing().setComment(head, CodeUnit.PLATE_COMMENT,
					"The main mode machine's dispatcher head. Takes up MODE_REQUEST as B_MAIN_MODE, "
					+ "sets M_MAIN_MODE to 0x80000000 >> B_MAIN_MODE, points the debug display at the "
					+ "mode's name, and jumps through the table at 0x00024354. Every state handler ends "
					+ "by branching back here or to fv2_mode_tick_return.");
			println("FV2Fix: fv2_mode_dispatch made at " + head);
		} else {
			println("FV2Fix: could not make a function at " + head);
		}

		// A handler's last instruction is a branch back to the dispatcher head or to the
		// shared epilogue, and neither ever comes back to it. Saying so stops the
		// decompiler walking on through every other state: without it a 208-byte handler
		// came out as 4451 lines holding the whole switch.
		for (long a : new long[]{0x0000ABF0L, 0x0000AC64L}) {
			Function g = getFunctionAt(toAddr(a));
			if (g != null) {
				g.setNoReturn(true);
				println("FV2Fix: " + g.getName() + " marked as never returning");
			}
		}

		// Each handler's last instruction is `b fv2_mode_dispatch` or `b fv2_mode_tick_return`.
		// To Ghidra that is a jump, so the decompiler inlines the target and walks on into
		// every other state. CALL_RETURN says what it really is -- a tail call -- and the
		// decompilation stops at it. Taken from the same address,end list that bounds the
		// bodies, so it only ever touches a handler's own last word.
		int overridden = 0;
		if (args.length >= 1) {
			try (BufferedReader r = new BufferedReader(new FileReader(args[0]))) {
				String line;
				while ((line = r.readLine()) != null) {
					line = line.trim();
					if (line.isEmpty() || line.startsWith("#"))
						continue;
					String[] p = line.split(",");
					if (p.length < 2)
						continue;
					Address last = toAddr(Long.parseLong(p[1].trim(), 16) - 4);
					Instruction in = getInstructionAt(last);
					if (in == null || !in.getMnemonicString().equals("b"))
						continue;
					Address t = in.getFlows().length == 1 ? in.getFlows()[0] : null;
					if (t == null || (t.getOffset() != 0x0000ABF0L && t.getOffset() != 0x0000AC64L))
						continue;
					in.setFlowOverride(FlowOverride.CALL_RETURN);
					overridden++;
				}
			}
			println("FV2Fix: " + overridden + " terminal branches made tail calls");
		}

		ReferenceManager rm = currentProgram.getReferenceManager();
		for (long[] dsp : DISPATCH) {
			Address at = toAddr(dsp[0]);
			Set<Address> good = new HashSet<>();
			for (int i = 0; i < dsp[2]; i++) {
				try {
					long t = currentProgram.getMemory().getInt(toAddr(dsp[1] + i * 4L)) & 0xFFFFFFFFL;
					good.add(toAddr(t));
				} catch (MemoryAccessException e) {
					println("FV2Fix: cannot read the table at " + toAddr(dsp[1] + i * 4L));
				}
			}
			int dropped = 0, kept = 0;
			for (Reference r : rm.getReferencesFrom(at)) {
				if (!r.getReferenceType().isComputed() || !r.getReferenceType().isJump())
					continue;
				if (good.contains(r.getToAddress()))
					kept++;
				else {
					rm.delete(r);
					dropped++;
				}
			}
			println("FV2Fix: " + at + " -> table " + toAddr(dsp[1]) + " x" + dsp[2]
					+ ": " + kept + " destinations kept, " + dropped + " dropped");
		}
	}
}
