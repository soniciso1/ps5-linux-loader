# Porting ps5-linux-loader to a new firmware (IDA + decrypted-firmware method)

This is the gzimbric/ps5-linux-porting method, adapted to our environment: we
do **not** derive values from live console dumps. We derive every
firmware-specific input by analysing the **decrypted** kernel and hypervisor
images in IDA (or capstone), then confirm each one against the
hardware-proven homolog in a known-good firmware (11.60 or 13.60).

Active case: **10.60** (uses the 11.x "Sonic" carrier: `hv_defeat_1160`,
`shellcode_kernel_1160`, `PS5_FW_IS_11XX`).

-------------------------------------------------------------------------------
## 0. Principles (do not skip)

1. **No blind offset substitution.** The single biggest source of past port
   failures was copying a value from another firmware or re-using a gadget
   address for a different field. Every value is derived from *this* firmware's
   binary and verified to be the right *function/gadget*, not just the right
   bytes.
2. **Keep the official coupled rest/wake architecture.** Do not invent a new
   wake wrapper. The userland order is fixed:
   `setup_env -> fetch_linux -> prepare_resume -> hv_defeat_1160 -> sleep(5)
   -> enter_rest_mode -> sleep forever`. There is no resumed-userland Linux
   launch phase.
3. **Component split is fixed:**
   - AP payload (`shellcode_break_hv`, PA 0x3ffff000): discovers the 16 GameOS
     VMCBs and disables nested paging. It does NOT stage or start Linux.
   - Kernel wake shellcode (`shellcode_kernel_1160`): patches the live HV exit,
     builds the identity map, installs the HV transition, copies linux_info +
     bzImage + initrd from the preinstalled aliases, rendezvouses all CPUs on
     vmmcall(1), enters Linux.
4. **Derivation source = decrypted firmware in IDA.** Where the upstream guide
   says "dump X from the console," we instead locate X in:
   - decrypted_modules/x86_kernel.elf
   - decrypted_modules/hypervisor.elf
   Load the kernel ELF in IDA forcing the generic `ELF64 for x86-64
   (Executable)` loader (NOT the PS4-module loader, which flattens .text into
   one DATA blob and finds zero functions). ktext base = 0xffffffff80210000.
5. **Verify by homolog, not by eye.** For every derived address, disassemble
   the enclosing function and confirm it is instruction-for-instruction the
   same function as the proven 11.60/13.60 one (masked sequence alignment, not
   raw byte compare -- RIP-relative displacements shift between images).

-------------------------------------------------------------------------------
## 1. Files to touch to add firmware 0xWXYZ (11.x engine)

| File | Change |
|---|---|
| source/offsets.c | new `offset_list off_WXYZ = { ... }` block |
| include/offsets.h | `extern offset_list off_WXYZ;` |
| source/utils.c | `case 0xWXYZ:` in set_offsets, with exact raw-build gate |
| include/config.h | add 0xWXYZ to PS5_FW_IS_11XX |
| source/main.c | routing already covered by PS5_FW_IS_11XX; confirm |
| source/hv_defeat_1160.c | add `case 0xWXYZ:` raw-gate in offsets_present() |
| shellcode_kernel_1160/shellcode_kernel_args_validate.h | accept 0xWXYZ |
| shellcode_kernel_1160/display_ops_1160.{c,h} | ps5_display_ops_WXYZ[29] + dispatch case |

-------------------------------------------------------------------------------
## 2. Firmware-specific inputs and how to derive each (IDA on decrypted fw)

All kernel offsets are relative to ktext base 0xffffffff80210000. HV values are
physical addresses in the HV image (flat image loaded at PA 0x62800000; file
offset = PA - 0x62800000).

### Kernel functions (find by signature, confirm body == 11.60 homolog)
- FUN_MEMCPY           forward/backward memcpy; 11.60 homolog body identical.
- FUN_PRINTF           standard printf prologue.
- FUN_SMP_RENDEZVOUS   smp_rendezvous(setup,action,teardown,arg).
- FUN_SMP_NO_RENDEVOUS_BARRIER  the REAL empty barrier function
  (push rbp; mov rbp,rsp; pop rbp; ret), referenced via its call thunk.
  DO NOT substitute a bare `ret` / RET_TRAMP here -- that is wrong for the
  11.x engine even though it "returns." Derive the real function's thunk.
- KERNEL_CFI_CHECK     __cfi_check; the wake blob patches its first byte to C3.
- FUN_TRANSMITTER_CONTROL, FUN_MP3_INITIALIZE, FUN_MP3_INVOKE,
  G_VBIOS              display path; confirm each body vs 11.60.
- HV_MP_STARTUP_CALL   the startup_mp call site hooked for the AP launch.
- MP_STARTUP_INIT_VECTOR  the init-vector thunk (jmp into the real routine).

### Kernel data / structures (find by RIP-relative xref context)
- ACPIGBL_FACS, IDT, COMMON_TSS, DATA_CAVE (large zeroed .data run,
  page-aligned, no code xref inside the 0x4000 used), KERNEL_CODE_CAVE
  (>=0x8000 of trailing NOP/pad at end of .text, page-aligned).

### Gadgets (disassemble at the offset, confirm EXACT instruction sequence)
Every GAD_*: the bytes at the offset must decode to precisely the documented
sequence with NO extra `pop`, side effect, or wild store before the final
`ret`/`iretq`. See the 8.xx audit notes for the four classic traps
(REP_MOVSB with pop rbp; STORE_RCX_RAX wild write; SET_DR7 with pop rbp;
missing clean store). Use GAD_STORE_RDI_RAX_RET + SET_DR7_POP_PADDING when a
clean form does not exist.

### HV values (physical, from hypervisor.elf)
- HV_HANDLE_VMEXIT_PA  the VMMCALL-specific VM-exit handler (NOT the general
  post-VMRUN return). Confirm the enclosing HV function body matches 11.60's
  (10.60 vs 11.60: 99.6% identical, benign codegen only).
- HV_CODE_CAVE_PA      int3 padding region (0x62806F00 across this family).
- HV_EFI_MAP_ROOT      kernel offset read by derive_h() at runtime to locate
  the live HV region; offset is applied to the two HV PAs above.

### WLAN firmware blob
- PS5_WIFI_FW_OFFSET/PS5_WIFI_FW_SIZE: locate the NXP IW620 combo blob
  (header 01 00 00 00 00 00 00 00 00 02 00 00 04 47 ba 08). Size 0x799BC in
  this family; the blob bytes differ per firmware so derive the offset fresh.

-------------------------------------------------------------------------------
## 3. Raw-build gate

set_offsets and hv_defeat_1160::offsets_present() both require
kernel_get_fw_version() to equal the exact per-build raw value (e.g. 10.60 =
0x10600006). Read this from the target's own kernel; refuse otherwise.

-------------------------------------------------------------------------------
## 4. Bounded bring-up order (one boundary per hardware run)

Mirror the repo's staged gates. Each stage changes ONE boundary and leaves a
durable/externally-observable result. Never introduce a new hook AND a new wake
blob in the same hardware run.

- Stage 0  loader-only build: reads both images, exits clean.
- Stage S  **vanilla S3 sanity**: build with hv_defeat disabled
  (`if (0 && hv_defeat_1160(...))`) AND prepare_resume's page-table / XO-text
  mutation also bypassed, to prove the bare S3 wake survives on this unit
  before blaming the chain. (The earlier 10.60 "vanilla" test did NOT bypass
  prepare_resume, so its "S3 is just broken" conclusion is unproven.)
- Stage 1  reversible per-page kernel aliases: install, verify, roll back.
- Stage 2  carrier preflight + awake #DB round trip + S3 resume provenance.
- Stage 3  arm AP/VMCB step: discover + change + verify exactly 16 VMCBs
  (read the oracle/stash diagnostics back: error code 1-5, self_count,
  targets, written, verified).
- Stage 4  full S3 wake/HV/Linux path -> authenticated SSH.
- Stage 5  display preamble/table -> physical HDMI.

-------------------------------------------------------------------------------
## 5. Known 10.60 status

- Offset table, routing, gadgets, HV handler, display ops, WLAN blob: all
  derived/verified clean against 11.60 this session EXCEPT the barrier below.
- **Defect found:** FUN_SMP_NO_RENDEVOUS_BARRIER was set to 0x20DB11 (a bare
  ret). Correct derived value is 0x2B82B8 (thunk to real
  smp_no_rendezvous_barrier at 0xaff820). This is downstream of the observed
  early-wake crash, so it is a latent fix, not the crash cause.
- Early-wake crash ("Fatal trap 12 / BIOS Fatal Error(0x2471)" right after
  "Resuming OS") still unexplained. The prepare_resume-inclusive vanilla test
  (Stage S above) has NOT yet been run; do it before concluding the devkit S3
  is inherently broken.
