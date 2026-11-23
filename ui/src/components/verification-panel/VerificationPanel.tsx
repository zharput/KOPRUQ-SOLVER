// Placeholder for the verification workspace (Phase 6). The MIDAS NX comparison logic stays in the
// C++ code base and will be reached through new CLI commands -- it is not reimplemented here.
export function VerificationPanel() {
  return (
    <div className="empty" data-testid="verification-empty">
      Verification workspace — not implemented yet (Phase 6). Test model selection, reference results, tolerance and PASS / FAIL / UNSUPPORTED
      reporting will call the existing C++ MIDAS NX comparison through the CLI.
    </div>
  );
}
