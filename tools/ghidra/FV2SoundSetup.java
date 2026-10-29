// FV2SoundSetup: seed Fighting Vipers 2's SOUND program for reading, before analysis.
//
// The image is the sound board's 68000 EPROM (epr-20600a.21, 512 KB), byte-swapped into the
// 68000's own big-endian order and based at 0x600000, which is where the board's map puts it
// (src/sound/soundboard.h). Raw binary has no entry point and no symbols, and unlike the game's
// PowerPC image there is no recompiler map to lean on -- but this program does not need one,
// because a 68000 carries its entry points in its own exception vector table.
//
// So: read the table, and make a function at every target inside the program ROM. The reset PC
// and the three interrupt handlers the board actually drives come out named. Everything else
// auto-analysis finds by following the calls.
//
// Run by tools/sound/import-sound.cmd through analyzeHeadless, as a -preScript, so that Ghidra's
// own analysis starts from these and not from nothing.
//
//@category FV2
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;

import java.util.LinkedHashMap;
import java.util.Map;

public class FV2SoundSetup extends GhidraScript {

	// Where the board maps the program ROM, and how big it is (soundboard.h: 600000 the program ROM).
	private static final long BASE = 0x600000L;
	private static final long SIZE = 0x80000L;

	// The 68000's vectors, by index. 0 is the initial stack pointer and not code; 25-31 are the
	// autovectored interrupt levels, which is where the SCSP's IPL lines land (only the FIRST
	// chip's interrupts reach the 68000 at all).
	private static String vectorName(int i)
	{
		switch (i) {
		case 1: return "reset";
		case 2: return "bus_error";
		case 3: return "address_error";
		case 4: return "illegal";
		case 5: return "div0";
		case 6: return "chk";
		case 7: return "trapv";
		case 8: return "privilege";
		case 9: return "trace";
		case 10: return "line_a";
		case 11: return "line_f";
		case 24: return "spurious";
		}
		if (i >= 25 && i <= 31) return "irq" + (i - 24);
		if (i >= 32 && i <= 47) return "trap" + (i - 32);
		return "vec" + i;
	}

	@Override
	public void run() throws Exception
	{
		// A 68000 has room for 256 vectors, but THIS program's table stops where its code starts:
		// the reset PC is 0x600120, so there are 72 longs and no more. Reading all 256 reads code
		// as longs, and a few of those happen to look like valid program addresses -- the first
		// run of this script made functions at 0x608068, 0x6002D4 and 0x600396 on that basis, in
		// the middle of real code. The reset PC is the table's own end marker, so use it.
		final long resetPc = getInt(toAddr(BASE + 4)) & 0xFFFFFFFFL;
		final int count = (resetPc > BASE && resetPc < BASE + SIZE) ? (int) ((resetPc - BASE) / 4) : 64;

		final LinkedHashMap<Long, String> entries = new LinkedHashMap<>();
		final LinkedHashMap<Long, Integer> shared = new LinkedHashMap<>();
		int looked = 0, outside = 0;
		for (int i = 0; i < count; i++) {
			final Address at = toAddr(BASE + 4L * i);
			if (!currentProgram.getMemory().contains(at))
				break;
			if (i == 0)
				continue;   // the initial stack pointer
			looked++;
			final long v = getInt(at) & 0xFFFFFFFFL;
			if (v < BASE || v >= BASE + SIZE || (v & 1) != 0) {
				outside++;
				continue;
			}
			// Named after the FIRST vector that points there, with a count: fifty-odd unused
			// vectors share one catch-all handler, and chaining their names gives a symbol
			// four hundred characters long that says nothing.
			if (!entries.containsKey(v)) {
				entries.put(v, "snd_" + vectorName(i));
				shared.put(v, 1);
			} else {
				shared.put(v, shared.get(v) + 1);
			}
		}
		for (Map.Entry<Long, Integer> e : shared.entrySet())
			if (e.getValue() > 1)
				entries.put(e.getKey(), entries.get(e.getKey()) + "_and_" + (e.getValue() - 1) + "_more");

		int made = 0, already = 0;
		for (Map.Entry<Long, String> e : entries.entrySet()) {
			final Address at = toAddr(e.getKey());
			disassemble(at);
			if (getFunctionAt(at) != null) {
				already++;
				continue;
			}
			if (createFunction(at, e.getValue()) != null)
				made++;
		}

		println("FV2SoundSetup: " + looked + " vectors read, " + outside + " pointing outside the ROM, "
		        + entries.size() + " distinct targets inside it, " + made + " functions created, "
		        + already + " already there");
		for (Map.Entry<Long, String> e : entries.entrySet())
			println(String.format("  %-28s %08X", e.getValue(), e.getKey()));
	}
}
