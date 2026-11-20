# KOPRUQ-SOLVER — Development Status

> Initial status assembled from user-provided development reports, **not** independently re-executed in this document. Claude must inspect repository state and refresh evidence before changing a milestone to COMPLETE.

## Development and reporting policy (effective 2026-10-16)
Full rules are in `CLAUDE.md`; summarized here per the user's instruction to
record the current policy in this document too:
- Work proceeds in approved work periods, each following
  PLAN → IMPLEMENT → BUILD → TEST → DEBUG → VERIFY → DOCUMENT → REPORT → WAIT
  FOR APPROVAL.
- A progress report is produced roughly every 30 minutes of active work (or
  at natural task boundaries if elapsed time isn't tracked precisely), after
  each major milestone, on any significant blocker, and before any
  high-impact decision. **Every progress report is a mandatory approval
  checkpoint** — development stops and waits for explicit approval ("Continue"
  / "Approved" / "Devam" / "Onaylıyorum"); silence is not approval.
- All progress reports are written in Turkish, in the fixed format specified
  in `CLAUDE.md` (Phase / Completed Work / Verification Results / Problems
  and Corrections / Development Status / Next Planned Tasks / Approval
  Request).
- This supersedes the previous looser "continuous work policy" of working
  through the whole roadmap unattended; where the two conflict, the more
  restrictive approval-gated rule governs.

## Current priority
**MIDAS NX JSON doğrulama (verification/models/01-05) — CNLD/Ecm Tablo 3.1
düzeltmeleri (`c14bf08`, `170db49`) ve Timoshenko kesme deformasyonu
MIDAS import'una bağlandı (`723ec78`, 2026-11-16), push/CI başlatılmadı.**

Kullanıcı modelleri 01-05'i MIDAS NX'te yeniden çalıştırıp `.xls`
(Excel 97-2003) formatında taze referans export'ları aldı; dosya adları
klasör adlarıyla eşleşecek şekilde yeniden düzenlendi. Tüm 5 model
`xlrd` ile okunarak ve solver'ın `midas-nx-json` CLI çıkışıyla elle
karşılaştırıldı:

- **Model 02 (Konsol_Fy), 03 (Konsol_Fz), 04 (Basit Kiriş Uniform):**
  Reaksiyonlar, eleman kesme/moment dağılımları MIDAS ile tam örtüşüyor.
  Deplasman karşılaştırması kesme deformasyonu eklenmeden önce: Model 02
  %4,63 fark, Model 03 %0,033 fark, Model 04 %0,019 fark (FORCE) /
  %0,041 fark (POINT) — farkın yönü ve büyüklüğü, o zaman henüz
  uygulanmamış Timoshenko kesme deformasyonu eksikliğiyle tutarlıydı.

- **Kesme deformasyonu (Timoshenko) MIDAS import'una bağlandı** (kullanıcı
  onayıyla, 2026-11-16): `MidasSectionLibrary.hpp`'ye her DBUSER şekli
  için klasik Timoshenko kesme düzeltme faktörüyle AsH/AsB eklendi (SB
  dikdörtgen: 5/6, izotropik; SR daire: 0,9; B kutu kesit: gövde/başlık
  alanı yaklaşımı, henüz doğrulanmış bir modelde kullanılmıyor).
  `MidasJsonImporter.hpp`, `SECT_BEFORE.USE_SHEAR_DEFORM` true olduğunda
  bunları `Section::Asy/Asz`'ye bağlıyor (Asy←AsB, Asz←AsH, mevcut
  Iy=IH/Iz=IB eşleşmesiyle aynı mantıkla); flag false ise hâlâ
  Euler-Bernoulli (Asy=Asz=0). Sonuç: Model 02'nin farkı %4,63'ten
  %1,89'a düştü; Model 03/04 (zaten neredeyse tam örtüşen) %0,23'lük
  küçük bir aşırı düzeltmeye kaydı. Fark tam sıfıra zorlanmadı —
  kalan ~%0,2-1,9 fark, kullanılan kesme düzeltme faktörünün MIDAS'ın
  kendi (bilinmeyen) değerinden küçük bir farkı olabileceğini gösteriyor,
  sayısal kanıt bu şekilde raporlanıyor.

- **Model 01 (Konsol_Fx) — bloke, kullanıcı talimatıyla mevcut veriyle
  ilerlendi:** Kullanıcı modeli yeniden çalıştırdı (taze export,
  2026-11-16 12:54-12:59), ancak aynı anomali tekrarlanıyor: tüm 4
  elemanda sabit Moment-y=-1,5 kN·m, Shear-z=0, ama mesnet MY=0 — saf
  eksenel yük altında bu denge koşuluyla açıklanamaz. Bu artık **export
  corruption değil** (önceki hipotez geçersiz); MIDAS'ın kendi
  çıktısında tekrarlanabilir bir denge ihlali olarak kayda geçiyor.
  Solver'ın kendi sonucu (sıfır moment, pür eksenel) denge açısından
  doğru; bu modelin referans verisiyle sayısal eşleşme aranmıyor.

- **Model 05 (Cerceve-1) — bloke, kullanıcı talimatıyla mevcut veriyle
  ilerlendi:** Geometrik olarak tam simetrik portal çerçeve (iki kolon,
  5 m, aynı kesit/malzeme, ikisi de ankastre), FORCE yük durumu da
  simetrik (kirişte tekdüze yayılı yük). Solver, beklenen simetrik
  sonucu veriyor (FZ=11750/11750, MY=±7555,77 — iç denge tanılamaları
  makine hassasiyetinde). MIDAS referansı **asimetrik**: FZ=10799,7/
  12700,3, MY=9898,957/-395,95 (toplam FZ=23500 tutuyor ama dağılım
  simetrik değil). Model 01'den bağımsız, ikinci bir MIDAS referans
  verisi tutarsızlığı; sayısal formülasyon bu asimetrik sonuca
  zorlanmadı.

CTest (Release ve Debug, ayrı ayrı çalıştırıldı): 225/230 PASS (6 yeni
test eklendi — `midas_section_shear_area_tests.cpp`,
`midas_json_importer_shear_tests.cpp`; kalan 5 başarısızlık önceden
teşhis edilmiş, ilgisiz `xmidas/deneme1.json` eksikliği, bu oturumda
değişmedi). Kullanıcı talimatıyla **model 06-11'e geçilmedi.**

**Güncelleme (2026-11-16/17) — Model 01 ve 05 çözüldü, Model 06 doğrulandı,
Phase 7H ve 7E.1 tamamlandı, Model 11/12 tam PASS:**

- **Model 01 ve 05'in MIDAS referans verisi kullanıcı tarafından düzeltildi**
  (MIDAS'ta yeniden çalıştırılıp taze export alındı, 2026-11-16 16:02-17:17).
  Her iki model artık **tam doğrulandı**: Model 01 saf eksenel davranış
  gösteriyor (önceki anomali gitti), reaksiyon/eksenel kuvvet tam eşleşiyor.
  Model 05'in model verisi gerçekten asimetrikmiş (farklı yükseklik/
  büyüklükte CNLD) — önceki "MIDAS asimetrik reaksiyon" bulgusu MIDAS hatası
  değil, model verisinin kendisiymiş; düzeltilmiş veriyle reaksiyon/kuvvet
  tam eşleşiyor.
- **Model 06 (Cerceve-2, yük kombinasyonlu) test edildi:** Deplasman 342/342,
  reaksiyon 36/36 (3 asıl yük durumu için) tam PASS; kalan N/A'lar tamamen
  `COMBO-1`/`COMBO-2` kombinasyonlarından (çözücü MIDAS `LCOM-GEN`
  kombinasyonlarını henüz içe aktarmıyor — bkz. `docs/SOLVER_ROADMAP.md`
  Phase 7G, başlanmadı).
- **Phase 7H — ModalSolver/ResponseSpectrumSolver'a rigid-link desteği
  (COMPLETE, commit `dd6898b`, 2026-11-17).** `LinearStaticSolver`'ın zaten
  doğrulanmış `RigidLinkReduction` (Guyan/DOF-eliminasyon) mekanizması
  ModalSolver'a ve ResponseSpectrumSolver'a genişletildi. Yeni
  `tests/unit/rigid_link_modal_tests.cpp` (7 test): frekanslar rigid-link'siz
  referans modelle bit-bit eşleşiyor, her modda slave node kinematiği
  (`u_slave=u_master+θ×e`) tam sağlanıyor, tam DOF uzayında kütle-
  ortonormalite doğrulandı. CLI'nin `modal --format midas-nx-json` komutu,
  importer'da hiçbir değişiklik gerekmeden, RIGD içeren gerçek bir MIDAS
  modelinde uçtan uca çalıştığı doğrulandı (el yapımı, gerçek `S_NODE` alan
  şemasıyla). EIGV/SPLC→`response_spectra` çevirisi ayrı, bloke bir alt-madde
  olarak bırakıldı (gerçek örnek veri — `xmidas/deneme1.json` — kullanıcı
  tarafından silinmiş, alan adları tahmin edilmedi).
- **Phase 7E.1 — MIDAS kiriş-kuvvet işaret kuralı düzeltmesi (COMPLETE,
  commit `ac1ecc9`, 2026-11-17).** Model 01'in saf eksenel statiğinden
  bağımsız olarak türetildi: MIDAS, N(x)/V(x)/M(x) diyagram değerini
  (konum-bağımsız, her iki uçta aynı işaret) raporluyor; çözücünün
  `local_end_force`'u ("eklemin elemana uyguladığı kuvvet") ise Axial/
  Shear/Torsion için I-ucunda ters işaretli (f_I=-N, f_J=+N), moment için
  tam tersi (f_I=+M, f_J=-M, kesme-kolu terimi `dM/dx=V` nedeniyle).
  `verification/ResultMatcher.cpp`'de tek noktada uygulandı, solver'a
  dokunulmadı. 2 bağımsız modelde (05: 147 satır, 06: 162 satır) **sıfır
  istisnayla** doğrulandı. Shear-y/Torsion/Moment-z aynı mantıkla
  genişletildi ama mevcut tüm modeller düzlemsel olduğu için gerçek veride
  hiç sınanmadı (açıkça disclosed).
- **Model 11 (Temprature-1) ve 12 (Temprature-2) — yeni, sıcaklık yükü
  (ETMP/TEMP-1,TEMP-2) içeren modeller, test edildi (2026-11-17):** Her ikisi
  de **tam PASS**: Model 11 deplasman 300/300, reaksiyon 24/24, kiriş kuvveti
  576/576; Model 12 (4 farklı kesit, 5 farklı mesnet tipi dahil) deplasman
  588/588, reaksiyon 60/60, kiriş kuvveti 1152/1152 — **sıfır FAIL, sıfır
  N/A** her ikisinde de. Bu, hem Phase 7T (uniform sıcaklık) hem Phase 7E.1
  (işaret kuralı) için tamamen farklı bir yük tipinde bağımsız bir çapraz
  doğrulama.
- **GUI V2.1 ikon/görsel revizyonu (COMPLETE, commit `c538687`, 2026-11-16).**
  Sol işlem butonları `QToolButton`'a geçirildi (ikon üstte/yazı altta), 5
  panel başlığına ikon eklendi, checkbox işareti ve log seviyesi ikonları
  Qt Resource System'e (`icons.qrc`) gömüldü (13 adet elle çizilmiş,
  Lucide-tarzı SVG). Solver/JSON/Excel mantığına dokunulmadı; `kopruq_verification_gui.exe`
  derlenip çalıştırılarak ekran görüntüleriyle doğrulandı.
- Üç commit de **push edilmedi** (kullanıcı onayı her checkpoint'te ayrı
  alındı, push talep edilmedi bu üç madde için — GUI V2.1 commit'i push
  edildi, 7H ve 7E.1 push edilmedi, henüz onay bekliyor).

**Qt6 Verification GUI V2.0 revizyonu — yerel olarak derlendi ve test edildi,
commit/push ve CI henüz onay bekliyor (2026-11-16).** Kullanıcının detaylı
"KOPRUQ-SOLVER — Qt6 Verification GUI V2.0 Revizyonu" talimatına göre:
sol tarafta sabit genişlikte 4 adımlı sıralı iş akışı sidebar'ı
(`AnalysisControlPanel`, artık `QWidget` tabanlı), yeni "Model Bilgileri
(JSON)" paneli (`ModelInfoPanel`, CLI'nin `reportMidasImport()` çıktısından
ayrıştırılan gerçek düğüm/eleman/malzeme/kesit/yük durumu sayıları ve
düğüm koordinatlarından hesaplanan model boyutları; desteklenmeyen alanlar
"N/A"), checkbox görünürlük hatası düzeltmesi (Fusion stili + açık mavi
arka plan + beyaz check.png görseli), yeni renk paleti
(`#0B1220`/`#131F31`/`#2563EB`/vb.), `MainWindow`'da açık bir
`WorkflowState` makinesi (InputReady→Validated→Solved→Compared) ve
JSON-dosyası-değişince tam sıfırlama / Excel-veya-checkbox-değişince
yalnızca karşılaştırmayı geçersiz kılma kuralları uygulandı. Backend'e
yalnızca katkısal iki alan eklendi: `MidasImportReport::elastic_links_rejected`
ve düğüm sınır kutusu (bounding box) — ikisi de yeni, bağımsız hesaplanan
GoogleTest doğrulamalarıyla (`ReportsElasticLinkRejectionCountAndNodeBoundingBox`,
`EmptyNodeSetHasNoBoundingBox`) kontrol edildi. Mevcut solver/karşılaştırma
algoritmaları değiştirilmedi. Yerel Windows derlemesi (`ninja-gui-release`)
temiz (uyarı yok), yerel CTest 244 testten 239'u PASS; kalan 5 başarısızlık
önceden var olan, bu oturumla ilgisiz `xmidas/deneme1.json` eksikliği
yüzünden (kullanıcının kendi yerel dosya yönetimi, git HEAD'de hâlâ
izleniyor, dokunulmadı). Windows paketleme/`release.yml` bu görevde
bilinçli olarak değiştirilmedi.

**Phase 7B's MIDAS ETMP (uniform temperature) wiring — COMPLETE/verified
(CI run `39247142910`, 219/219 Debug/Release/ASan, 2026-10-19).** Per the
user's explicit follow-up request, the MIDAS importer now uses Phase 7T's
thermal-load capability instead of rejecting `TU`/`ETMP`: the real
sample's 40 `ETMP` records import and the model **solves successfully**
with the thermal load actually active (not a zero no-op -- material
thermal expansion is now also assigned). `GTMP`/`BTMP` (gradient/beam-
section temperature) are recognized and explicitly rejected when present
(0 records in this sample); MIDAS's T1/T2 top-fiber sign convention could
not be confirmed from accessible documentation, so it was not guessed at.

**Phase 7B, 7M and 7S (increment 1) — COMPLETE/verified (CI run
`39227258173`, 199/199 Debug/Release/ASan, 2026-10-19).** The GitHub
Actions "no runner assigned" blocker (present 2026-10-18 through the
morning of 2026-10-19) was resolved by the user increasing the account's
Actions quota; every commit accumulated while it was blocked
(`0823f8f` through `f10dca0`, plus the fix in `59b0035`) has now been
compiled and tested for the first time, in one CI run. One real bug was
caught (Phase 7S's centroid divisor, see below); everything else passed
on the first try. The real `xmidas/deneme1.json` bridge model now
imports, validates and **solves successfully end to end** with
KOPRUQ-SOLVER's own `LinearStaticSolver`.

**Phase 7T (temperature loads) — COMPLETE/verified (CI run `39237884916`,
215/215 Debug/Release/ASan, 2026-10-19).** This completes the approved
7M -> 7S -> 7T implementation order in full. Two genuine issues were
caught and fixed during verification (session log below): a test-design
flaw (a scale-normalized equilibrium ratio that reads exactly 1.0 for
any nonzero reaction when the applied-load resultant is exactly zero by
design) and a real, pre-existing solver robustness gap (`solveCases`
crashed on a model with zero free DOFs -- `SimplicialLDLT::compute()` on
a 0x0 matrix aborted in Debug builds specifically, masked in Release).

**Phase 7A — MIDAS NX JSON import audit — COMPLETE (2026-10-18, audit
only, no code changed).** `docs/MIDAS_JSON_DATA_AUDIT.md` and
`docs/SOLVER_CAPABILITY_MATRIX.md` record a full inventory of the real
`xmidas/deneme1.json` export and a capability-by-capability gap analysis
against the current solver.

Phase 5 — COMPLETE IN FULL, 5.1-5.6 (verified, CI run `39104401638`,
167/167 Debug/Release/ASan). Truss elements (5.2), rigid end offsets
(5.3), spring/link elements including bearings (5.4), general rigid
links/multi-point constraints (5.5) and the Timoshenko beam option (5.6)
are all implemented. 5.5 and 5.6 were initially deferred as out of scope
for the 2026-10-17 work period, then explicitly requested and completed on
2026-10-18 per the user's follow-up instruction. Phase 6 (6.1, 6.1b, 6.2,
6.2b) remains complete and unaffected; 6.3 is cancelled per the user's
2026-10-17 scope decision. See "Next autonomous action" for the current
approval checkpoint (stopped before Phase 7).

**Phase 3.2 requalification — COMPLETE (verified on CI, run `38872157394`,
commit `1ed5ae4`).** Next: Phase 4, which has **not started** — its first
increment needs user approval of scope and numerical formulation (see "Next
autonomous action"). Full verified/unverified capability list:
`docs/REQUALIFICATION_REPORT.md`.
The local build blocker (ESET `@Trojan.ML/Augur` false positive, see below)
is still unresolved on the developer machine, but is now **sidestepped
legitimately via CI**: per the user's decision (2026-10-16), all Debug and
Release builds/tests run on a GitHub-hosted Windows runner
(`.github/workflows/ci.yml`, private repository), so no compiled binary is
created or executed locally. The local ESET exclusion request to IT remains
open and would restore local builds if granted.

**Verification source of truth from now on:** GitHub Actions run results
(Debug, Release, ASan and static-analysis jobs) for the pushed commit, not
local `build/` trees. Locally, only compile-to-`.obj` checks are run (no
linking, no executables).

## Established reported history
| Milestone | Status | Evidence / qualification |
|---|---|---|
| Phase 0–2 | Previously implemented | Scaffold, Beam3D, DOF numbering and sparse assembly; historical regression tests subsequently lost |
| Phase 3.1 | Previously implemented | Linear static solver, portal dense reference, optimized sparse Kff; historical 22/22 PASS no longer counts as current coverage |
| Phase 3.2 | Partially implemented | Reaction/element force recovery; historical 25 tests lost; two current ElementRecovery tests reported PASS |
| Phase 3.2R1 | COMPLETE (verified) | New Beam3D tests; Debug 7/7, Release 7/7; commit `907ea32` |
| Phase 3.2R2 | COMPLETE (verified) | New assembly tests; Debug 12/12, Release 12/12; commits `07f7344`, `f050918` |
| Phase 3.2R3 | COMPLETE (verified) | New linear static solver tests; Debug 22/22, Release 22/22 |
| Phase 3.2R4 | COMPLETE (verified) | Implemented + requalified strain-energy recovery; Debug 26/26, Release 26/26; commit `643c829` + test commit below |
| Phase 3.2R5 | COMPLETE (verified) | Benchmarks + linear-scaling fix, ASan, `/analyze` gate, diagnostics fields; Debug/Release/ASan 30/30 on CI run `38872157394`; commits `1a42c61`, `734b2fc`, `c564dc3`, `506a8be`, `1ed5ae4` |
| Phase 4.1 | COMPLETE (verified) | Member loads, self-weight, load cases/combinations, `fixed`; Debug/Release/ASan 41/41 on CI run `38874706685`; commits `4fefc25`, `9389ab3` |
| Phase 4.1b | COMPLETE (verified) | Projected-length option for global member loads; Debug/Release/ASan 46/46 on CI run `38878331252`; commits `08af7ed`, `7036b61`, `cecbe0d` |
| Phase 4.2 | COMPLETE (verified) | JSON model I/O (`kopruq-model` v1), results (`kopruq-results` v1), CLI `solve`; Debug/Release/ASan 61/61 on CI run `38884699955`; commits `2d9e4da`, `b3b6ab0`, `24f1c65` |
| Phase 4.3 | COMPLETE (verified) | Member point forces/moments, distributed moments, JSON `point`/`kind`; force-equilibrium metric fix; Debug/Release/ASan 76/76 on CI run `38887606677`; commits `48ad305`, `b2a605f`, `62cd955` |
| Phase 5.1 | COMPLETE (verified) | Beam end releases by static condensation; unstable-combination rejection; JSON `releases`; Debug/Release/ASan 83/83 on CI run `38894147560`; commits `4dee2b5`, `cbef2e8`, `41aec8a` |
| Phase 4.4 | COMPLETE (verified) | `validateModel()`/`validateModelOrThrow()` for direct C++ API callers; `Restraints` promoted to namespace scope; Debug/Release/ASan 97/97 on CI run `38896544673`; commits `dba9837`, `a942083` |
| Phase 6.1 | COMPLETE (verified) | Consistent mass matrix, Guyan mass condensation for released ends, `ModalSolver`; Debug/Release/ASan 104/104 on CI run `38902536528`; commits `63402a5`, `d12a40a`, `bca8206` |
| Phase 6.1b | COMPLETE (verified) | JSON `kopruq-modal-results` v1, CLI `modal` command; Debug/Release/ASan 110/110 on CI run `38906210839`; commits `86f90d1`, `31fb203` |

## Known blocking issue (2026-10-16, Phase 6.2) -- GitHub Actions runner provisioning, ACTIVE
- Branch `ci/phase6.2-response-spectrum`, commit `a7997c6`: every CI run since
  ~14:00 UTC fails in every job (`ninja-release`, `ninja-debug`, `ninja-asan`,
  `ninja-analyze`) within 2 seconds of being created, with `runner_id: 0`,
  `runner_name: ""` and an empty `steps` list -- i.e. no runner was ever
  assigned and nothing executed, so there is no build/test failure to
  diagnose. Fetching each job's logs returns an Azure Blob Storage
  `BlobNotFound` error, consistent with no log ever being written.
- Tried: a rerun of the stuck run (`POST .../rerun`, attempt 3, same result)
  and a brand-new run from an empty commit (`a7997c6`) -- both failed
  identically.
- Runs earlier the same day (through ~13:33 UTC, including this exact
  workflow and commit chain) completed normally, so this is not the
  workflow file, the code, or a one-off transient blip; it started
  partway through this session's heavy same-day usage (dozens of
  4-job Windows-runner CI runs).
- Likely cause: a GitHub Actions usage/spending limit or included-minutes
  quota reached for the account (Windows runners bill at a 2x multiplier,
  and today's session ran many full 4-job matrices). Could not confirm via
  API: `GET /users/zharput/settings/billing/actions` returned 404 with this
  token's permissions.
- This requires the user's action (GitHub account billing/Actions settings),
  per project rules on external/billing matters -- not something this
  session can resolve or safely retry around. Stopped retrying once the
  pattern was confirmed identical across a rerun and a fresh run, rather
  than continuing to burn further attempts against the same limit.
- Local builds remain separately blocked by the ESET `@Trojan.ML/Augur`
  false positive (see below); until one of these is resolved, no
  Debug/Release/ASan verification is currently possible for new changes.
  Phase 6.2's code sits on `ci/phase6.2-response-spectrum`, unmerged into
  `main`, pending a clean CI run.

### Update 2026-10-17 -- persists on a new day, not a daily quota
Retried with a fresh empty commit (`96447ee`) on 2026-10-17: run `39020322855`
failed identically in all 4 jobs -- 2-second lifetime, `runner_id: 0`, zero
steps. Since this is a new UTC day and the pattern is unchanged, it is not a
daily-reset quota; most likely a monthly Actions spending/included-minutes
limit that will not clear itself before the user acts (or the next billing
cycle). Stopped retrying again; waiting for the user to check
https://github.com/settings/billing (or the repo's organization billing
page) before attempting CI again.

## Known blocking issue (worked around, not root-caused)
- CMake/vcpkg compiler detection fails with permission denied for `CMakeCXXCompilerId.exe`
  when vcpkg's manifest step re-runs its own compiler detection during
  reconfigure. Worked around for the existing `build/debug` and
  `build/release` trees by reconfiguring with `-DVCPKG_MANIFEST_INSTALL=OFF`
  (dependencies were already present in `vcpkg_installed`). This defers but
  does not fix the underlying permission denial.
- Do not disable security protections or terminate unrelated processes.

## New blocking issue (2026-10-16, R5 start) — ACTIVE, NOT A CODE DEFECT
While starting R5, newly linked `.exe` files in `build/debug` and
`build/release` began intermittently coming out **0 bytes** (link reports
success, or fails with `LNK1104: cannot open file`) or **disappearing
entirely** minutes after a successful build, with no source change and no
process of mine touching them. Diagnosis performed:
- `Get-MpComputerStatus`: Windows Defender real-time protection and
  antivirus are **disabled**.
- `Get-CimInstance ... AntiVirusProduct`: three registered security
  products — Windows Defender, **FortiClient**, **ESET Security**.
- Smart App Control (`VerifiedAndReputablePolicyState`) is **off** (0).
- The affected file was not different each time in a fixed way: retrying a
  build of the *same* target repeatedly eventually produces a valid,
  correctly-sized executable, but a *different* target already built
  earlier would then turn up 0 bytes or missing on the next check, with no
  rebuild of mine touching it. This points to a background process
  scanning/quarantining newly-created executables asynchronously, most
  consistent with FortiClient and/or ESET Security's real-time/EDR
  protection reacting to a short burst of many new small `.exe` files being
  created (a pattern these products commonly treat as a
  suspicious/ransomware-like indicator), not a build-script or compiler
  problem.
- This is **confined to `build/`**, which is not version-controlled; no
  source file, test file, or Git history was affected. `git status` stayed
  clean except for the intentional, not-yet-verified R5 benchmark edit.
- Per project rules, I did not disable Defender/FortiClient/ESET, did not
  terminate their processes, and did not request any such change myself.
  Stopped further rebuild attempts once the pattern escalated from
  individual 0-byte files to whole executables disappearing, rather than
  keep retrying into a worsening state.
- **This requires user/IT action to resolve durably**: most likely an
  exclusion for `C:\_ZHarput_Data\KOPRUQ-SOLVER\build\` (or the whole
  repository) in FortiClient's and/or ESET's real-time-scan/EDR policy.
  That is a security-setting change outside this session's authority to
  make.
- Until resolved, R5 benchmark numbers and a fresh CLI smoke check cannot be
  reliably captured. R1-R4's CTest evidence is unaffected: it was captured
  earlier in this session while the build tree was still stable, and
  `tests/unit/*` test logic does not depend on the CLI/benchmark apps that
  are being hit.

### ROOT CAUSE CONFIRMED (2026-10-16, ~09:10)
ESET Security's own detection log
(`C:\ProgramData\ESET\ESET Security\Logs\virlog.dat`, read with shared
access) contains **79 detections, all named `@Trojan.ML/Augur`** (ESET's
machine-learning "Augur" generic detection), with MSVC `link.exe` as the
triggering process and detection engine `34000 (20261015)`. Detected files
are exactly our build outputs — every `kopruq_*.exe` in `build/debug` and
`build/release` — plus CMake's `CMakeCXXCompilerId.exe`/
`CMakeCCompilerId.exe` probes in `build/r2-debug*`,
`build/requalification-r2-debug` and
`C:\_ZHarput_Data\Tools\vcpkg\buildtrees\detect_compiler\`. This is also
the root cause of the original R2 `CMakeCXXCompilerId.exe: Permission
denied` blocker. ESET's `ransomlog.dat` and `hipslog.dat` are empty, so the
ransomware shield and HIPS were not involved; Fortinet is not implicated.
Earlier text below speculating about FortiClient or a ransomware-pattern
heuristic is superseded by this finding.

Assessment: false positive on freshly compiled, unsigned, low-prevalence
binaries. Correct fix: an ESET PROTECT detection exclusion for
`C:\_ZHarput_Data\KOPRUQ-SOLVER\build\` and
`C:\_ZHarput_Data\Tools\vcpkg\buildtrees\` (and optionally a false-positive
submission to ESET), applied by IT. Not attempted from this session:
disabling ESET, killing its processes, or altering binaries/build output
locations to evade the detection would all be bypassing a security
control and are out of scope.

### Update (same day, after user approval to continue)
Rebuilt `build/debug` and confirmed, at that moment, all 5 CTest
executables present with correct nonzero sizes and a clean **26/26 PASS**
(`kopruq_solver_cli.exe`, not a CTest target, was 0 bytes at the same time).
Shortly after, with **no build command issued in between** — only `ls`/
`rm`/`stat` — 3 of those same 5 just-verified executables
(`kopruq_assembly_requalification_tests.exe`,
`kopruq_beam3d_requalification_tests.exe`,
`kopruq_linear_static_solver_requalification_tests.exe`) had disappeared
entirely and a 4th (`kopruq_recovery_requalification_tests.exe`) had gone to
0 bytes, leaving only 2 of 8 built executables intact. This rules out a
pure link-time race: something is removing/truncating already-written,
already-verified executables in this directory **asynchronously, after the
fact**, independent of any build activity on my part. Per the stricter
retry rule just added to `CLAUDE.md`, stopped immediately rather than
continuing to rebuild into a worsening state, and reported it as a blocker
instead. Source tree remained clean throughout (confirmed via `git
status`); no data loss.

## Test integrity
- Historical 25-test suite was overwritten and cannot be claimed recovered.
- **Verified 2026-10-16** (after R2 changes, commit `f050918`): Debug 12/12,
  Release 12/12, including the two preserved `ElementRecovery` tests, the
  Beam3D requalification suite (R1), and the new assembly requalification
  suite (R2).
- **Verified 2026-10-16 on CI** (GitHub Actions run `38861593856`, commit
  `1a42c61`, windows-2022, MSVC + Ninja + vcpkg at the manifest baseline):
  Debug **26/26**, Release **26/26** (`ctest -N` inventory: 26 tests in both),
  i.e. 2 preserved `ElementRecovery` + 5 Beam3D (R1) + 5 assembly (R2) +
  10 linear static (R3) + 4 recovery (R4).
- **Verified 2026-10-16 on CI** (run `38872157394`, commit `1ed5ae4`):
  Debug **30/30**, Release **30/30**, AddressSanitizer **30/30** —
  the 26 above + 4 `SolverDiagnostics` (R5).
- **Verified 2026-10-16 on CI** (run `38874706685`, commit `9389ab3`):
  Debug **41/41**, Release **41/41**, AddressSanitizer **41/41** —
  the 30 above + 11 `MemberLoads` (Phase 4.1). Static analysis: 0 blocking.
- **Verified 2026-10-16 on CI** (run `38878331252`, commit `cecbe0d`):
  Debug **46/46**, Release **46/46**, AddressSanitizer **46/46** — the 41
  above + 5 `ProjectedLoads` (Phase 4.1b). Static analysis: 0 blocking.
- **Verified 2026-10-16 on CI** (run `38884699955`, commit `24f1c65`):
  Debug **61/61**, Release **61/61**, AddressSanitizer **61/61** — the 46
  above + 13 I/O GoogleTest cases (`Sha256`, `ModelJson`, `ResultsJson`,
  `Cli`) + 2 CTest runs of the real CLI executable. Static analysis: 0
  blocking.
- **Verified 2026-10-16 on CI** (run `38887606677`, commit `62cd955`):
  Debug **76/76**, Release **76/76**, AddressSanitizer **76/76** — the 61
  above + 15 `PointLoads`. Static analysis: 0 blocking.
- **Verified 2026-10-16 on CI** (run `38894147560`, commit `41aec8a`):
  Debug **83/83**, Release **83/83**, AddressSanitizer **83/83** — the 76
  above + 7 `Releases`. Static analysis: 0 blocking.
- **Verified 2026-10-16 on CI** (run `38896544673`, commit `a942083`):
  Debug **97/97**, Release **97/97**, AddressSanitizer **97/97** — the 83
  above + 14 `Validate`. Static analysis: 0 blocking.
- **Verified 2026-10-16 on CI** (run `38902536528`, commit `bca8206`):
  Debug **104/104**, Release **104/104**, AddressSanitizer **104/104** — the
  97 above + 7 `Modal`. Static analysis: 0 blocking.
- **Verified 2026-10-16 on CI** (run `38906210839`, commit `31fb203`):
  Debug **110/110**, Release **110/110**, AddressSanitizer **110/110** — the
  104 above + 5 `ModalResultsJson`/`Cli` GoogleTest cases + 1 CTest run of
  the real CLI executable (`cli.modal_portal_frame_example`). Static
  analysis: 0 blocking.
- Every new test is in a dedicated file; inventory audited before/after each
  milestone.

## Session work log
### 2026-10-16 — Phase 3.2R2 completion
- Git HEAD / working tree: started at `907ea32` with uncommitted R2 files; ended at `f050918` on `main` (merged from `requalification/r2-assembly`, fast-forward)
- Current task: unblock R2 build, verify independent assembly tests, fix any defects
- Commands executed: `cmake -S . -B build/{debug,release} -DVCPKG_MANIFEST_INSTALL=OFF`, `cmake --build`, `ctest --output-on-failure`, CLI smoke checks (`kopruq_solver_cli --version|--help|<unknown>`)
- Source/test files changed: `tests/unit/assembly_requalification_tests.cpp` (new), `CMakeLists.txt` (wire new target into `gtest_discover_tests`), `include/kopruq/assembly/DofManager.hpp` (production fix)
- Debug focused / full CTest: 5/5 new assembly tests PASS standalone; 12/12 full suite PASS
- Release focused / full CTest: 12/12 full suite PASS
- Independent numerical reference and error: test-local Euler–Bernoulli 12×12 reference matched `MatrixAssembler` output to relative Frobenius error < 1e-9 (coefficient scale) / machine precision; independent 18×18 two-element shared-node reference matched to relative Frobenius error < 1e-11
- Root cause of any failures:
  - Build: vcpkg manifest install re-ran compiler detection and hit a `Permission denied` on `CMakeCXXCompilerId.exe`; worked around (not fixed) by skipping manifest install on already-populated build trees.
  - Test harness (4 defects, corrected, not production bugs): uninitialized dynamic-size `Eigen::MatrixXd` zero-matrix in the local reference; reference used `G = 80e6` instead of the model's `G = E/(2(1+nu))`; 18×18 DOF map assumed node insertion order instead of `DofManager`'s sorted-ID order; an `EXPECT_THROW` wrapped a lambda it never invoked.
  - Production (1 defect, fixed in `DofManager.hpp`): duplicate-node-id check tested `offsets_` membership before `offsets_` was populated, so duplicate node IDs were silently accepted instead of raising `AssemblyError`.
- Milestone status: **COMPLETE**
- Next unblocked roadmap task: Phase 3.2R3 — linear static solver requalification

### 2026-10-16 — Phase 3.2R3 completion
- Git HEAD / working tree: started at `b364fc3`; new test file plus `CMakeLists.txt` wiring, no uncommitted changes outstanding at session end
- Current task: independently requalify `LinearStaticSolver` per R3 acceptance criteria
- Commands executed: `cmake --build build/{debug,release}`, direct run of `kopruq_linear_static_solver_requalification_tests.exe`, `ctest` full suite in both configs, CLI smoke checks
- Source/test files changed: `tests/unit/linear_static_solver_requalification_tests.cpp` (new, 10 tests), `CMakeLists.txt` (wire new target into `gtest_discover_tests`)
- Debug focused / full CTest: 10/10 new tests PASS standalone; 22/22 full suite PASS
- Release focused / full CTest: 22/22 full suite PASS
- Independent numerical reference and error: analytical Euler–Bernoulli/torsion cantilever formulas (axial, flexural, torsional, combined) matched to relative error < 1e-9; independent dense 18-DOF two-element portal-frame (column + cantilevered beam), assembled/partitioned/solved with a fresh Eigen `LDLT` dense solve (no production assembly/DOF/solver code), matched production displacements and node-1 reactions to relative error < 1e-8/1e-9; `reduceKff`/`assembleLoads` checked against hand-built sparse systems and DOF maps to machine precision
- Root cause of any failures: none — all 10 new tests passed on first run; no production or test-harness defects found in this milestone
- Milestone status: **COMPLETE**
- Next unblocked roadmap task: Phase 3.2R4 — reaction, element-force and energy recovery requalification

### 2026-10-16 — Phase 3.2R4 completion
- Git HEAD / working tree: started at `63c97c8`; ended with all changes committed on `main`
- Current task: requalify reaction/element-force/energy recovery, scoped to what R3 did not already cover
- Commands executed: `cmake --build build/{debug,release}`, direct run of `kopruq_recovery_requalification_tests.exe`, `ctest` full suite in both configs, CLI smoke checks; code inspection (`grep` for `SimplicialLDLT`/`compute`/`solve` in `LinearStaticSolver.hpp`) to verify recovery does not repeat factorization
- Source/test files changed: `include/kopruq/solvers/LinearStaticSolver.hpp` (production fix — implemented previously-dead `strain_energy`/`element_strain_energy`/`energy_error` fields), `tests/unit/recovery_requalification_tests.cpp` (new, 4 tests), `CMakeLists.txt` (wire new target into `gtest_discover_tests`)
- Debug focused / full CTest: 4/4 new tests PASS standalone; 26/26 full suite PASS
- Release focused / full CTest: 26/26 full suite PASS
- Independent numerical reference and error: Clapeyron's-theorem external-work formulas for axial/combined-load cantilevers; an inclined (non-axis-aligned) 3-4-5 beam with a purely axial applied load, displacement/reaction/energy checked via vector geometry, not any production transform; a two-element portal frame with per-element local/global end forces recomputed via independently-written rotation/stiffness helpers. All energy-consistency checks landed at floating-point roundoff (< 1e-9–1e-10 relative), consistent with strain energy being an exact identity (UᵗKU = Σ element quadratic forms), not an approximate check.
- Root cause of any failures: none at the test level — all 4 new tests passed on first run. One production gap found and fixed: `AnalysisResults`/`ElementResults` declared energy fields that `recover()` never populated (always zero); implemented per the roadmap's explicit "energy consistency" acceptance criterion.
- Milestone status: **COMPLETE**
- Next unblocked roadmap task: Phase 3.2R5 — performance, robustness and release gate

### 2026-10-16 — Phase 3.2R5 start — BLOCKED (environment, not code)
- Git HEAD / working tree: `502d51e`; one uncommitted, not-yet-verified
  change: `apps/benchmarks/solver_benchmark.cpp` now also times
  `LinearStaticSolver::recover()` per run (previously only
  assembly/reduction/symbolic/numeric/solve were measured), addressing the
  roadmap's "measure recovery separately" criterion. Not committed because
  it has not been successfully built and run end-to-end yet.
- Current task: reproducible benchmarks at increasing size with
  assembly/Kff/factorization/solve/recovery measured separately, full
  Debug/Release CTest, CLI smoke test, requalification report.
- Commands executed: multiple `cmake --build` attempts/retries on
  `build/debug` and `build/release`; one experimental, reverted
  `-DVCPKG_APPLOCAL_DEPS=OFF` reconfigure (it stopped vcpkg from copying
  required runtime DLLs next to Release test executables, causing them to
  fail at runtime with no DLL — reverted back to `ON` immediately; noted
  here so a future session does not retry the same dead end);
  `Get-MpComputerStatus`, `Get-CimInstance ... AntiVirusProduct`, registry
  check of `VerifiedAndReputablePolicyState` for diagnosis.
- Source/test files changed: `apps/benchmarks/solver_benchmark.cpp`
  (uncommitted).
- Debug focused / full CTest: last clean confirmation before the new
  blocking issue appeared was 26/26 (same run documented in the R4 entry
  above). Could not get a second clean confirmation afterward because
  individual executables kept coming out 0 bytes or disappearing (see "New
  blocking issue" above) — not because any test logic changed or failed.
- Release focused / full CTest: not re-confirmed this session after the
  issue appeared, for the same reason.
- Independent numerical reference and error: none produced yet for R5 —
  blocked before any benchmark run completed cleanly.
- Root cause of any failures: environmental, not a code defect — see "New
  blocking issue" above. Most likely FortiClient/ESET real-time/EDR
  protection reacting to many new small executables being created in a
  short window.
- Milestone status: **BLOCKED** (genuine external blocker; stopped per
  project rules rather than continuing to retry into a worsening state or
  touching security software).
- Next unblocked roadmap task: none — paused for user/IT input on the new
  blocking issue before continuing R5.

### 2026-10-16 — Phase 3.2R5: CI verification path + first benchmarks
- Git HEAD / working tree: `1a42c61` on `ci/github-actions`, fast-forwarded
  into `main` after the CI run below passed; working tree clean.
- Current task: restore a trustworthy Debug/Release verification path
  without bypassing ESET, then take R5's first benchmark measurements.
- Source/test files changed: `.github/workflows/ci.yml` (new);
  `apps/benchmarks/solver_benchmark.cpp` (recovery phase now timed
  separately — previously uncommitted, now verified by CI).
- Debug full CTest (CI): 26/26 PASS. Release full CTest (CI): 26/26 PASS.
- CLI smoke (CI, Release): `--version` → `KOPRUQ-SOLVER 0.1.0`, exit 0;
  `--help` → usage, exit 0; unknown command → `Unknown command: bogus`,
  exit 2. PASS.
- Benchmarks (CI runner, Release, median of 5 after warm-up; straight
  cantilever chain, node 1 fixed, unit load at the tip):

  | elements | DOFs | assembly ms | Kff reduction ms | symbolic ms | numeric ms | solve ms | recovery ms | normalized residual |
  |---:|---:|---:|---:|---:|---:|---:|---:|---:|
  | 100 | 606 | 0.247 | 0.093 | 0.136 | 0.018 | 0.008 | 0.366 | 3.5e-18 |
  | 1,000 | 6,006 | 3.086 | 1.119 | 1.375 | 0.237 | 0.086 | 5.317 | 1.1e-18 |
  | 10,000 | 60,006 | 99.30 | 7.06 | 11.95 | 2.73 | 0.870 | 253.1 | 3.1e-19 |

  `kopruq_sparse_assembly_benchmark` independently: 0.269 / 2.94 / 97.97 ms
  median for 100 / 1,000 / 10,000 elements.
- **Engineering finding (R5 performance gate): assembly and recovery scale
  quadratically.** From 1,000 to 10,000 elements (10×), Kff reduction,
  factorization and solve grow ~6–12× (linear, as expected for this banded
  sparse system), but assembly grows ~32× and recovery ~48×. Code
  inspection explains it: `MatrixAssembler::assemble()` and
  `LinearStaticSolver::recover()` locate each element's two nodes by a
  linear scan over `m.nodes` (and materials/sections by linear scan), and
  `recover()` repeats node scans per load and per reaction — O(elements ×
  nodes). Numerical results are unaffected (residuals ~1e-18); this is a
  scalability defect only. Not yet fixed.
- Milestone status: **R5 INCOMPLETE** — verification path restored,
  benchmarks captured, quadratic-scaling defect identified.
- Next unblocked roadmap task: replace the linear node/property lookups
  with ID→index maps built once per call, re-verify 26/26 on CI, and
  confirm near-linear scaling.

### 2026-10-16 — Phase 3.2R5: scaling fix, inventory audit, report
- Git HEAD: `734b2fc` (code) + docs commit, on `ci/perf-id-lookups`,
  fast-forwarded into `main` after CI passed.
- Source files changed: `include/kopruq/assembly/MatrixAssembler.hpp`,
  `include/kopruq/solvers/LinearStaticSolver.hpp` — element node/material/
  section lookups now via ID→pointer indices built once per call
  (`detail::index_by_id`, last match wins, as the old scans did). Local
  check: affected translation units compiled to `.obj` only (no link, no
  executable), no warnings.
- Debug full CTest (CI run `38863504705`): 26/26 PASS.
- Release full CTest (same run): 26/26 PASS. CLI smoke PASS.
- Benchmarks at 10,000 elements: assembly 99.30 → 19.44 ms, recovery
  253.1 → 31.41 ms; 1,000 → 10,000 growth now ~9.9× (assembly) and ~11.3×
  (recovery), i.e. linear. Residuals bit-identical before/after
  (abs 0.0467754, normalized 3.14632e-19).
- Test inventory audit: 26 `TEST` cases in 5 files = 26 CTest tests; no
  disabled/skipped tests; no test file deleted in Git history;
  `core_tests.cpp` unchanged since `1f67b16`.
- Gaps found (not fixed, recorded in the report): `residual_norm`,
  `relative_residual`, `fixed`, `element_equilibrium_error`,
  `element_moment_equilibrium_error` never populated; `NonFiniteSolution`
  never produced.
- Milestone status: **R5 INCOMPLETE** — only sanitizers/static analysis
  remain.

### 2026-10-16 — Phase 3.2R5 completion: ASan, static analysis, diagnostics
- Git HEAD: `1ed5ae4` (+ this docs commit) on `ci/r5-asan-analyze`,
  fast-forwarded into `main`.
- Files changed: `CMakePresets.json` (`ninja-asan`, `ninja-analyze`),
  `.github/workflows/ci.yml` (4-job matrix, analyze gate),
  `include/kopruq/solvers/LinearStaticSolver.hpp`,
  `include/kopruq/results/AnalysisResults.hpp`,
  `tests/unit/solver_diagnostics_tests.cpp` (new, 4 tests), `CMakeLists.txt`.
- AddressSanitizer (RelWithDebInfo + `/fsanitize=address`): full CTest
  26/26 (run `38871473340`) and 30/30 (runs `38871760106`, `38872157394`),
  no sanitizer reports.
- `/analyze` (third-party angle-bracket headers excluded): first run 118
  unique warnings, second 142 (new test file) — every one C6326 on a
  GoogleTest assertion line in `tests\` (macro expands to
  `switch (0) case 0: default:`); 0 in `include\kopruq` and `apps\`. Job
  now fails on anything else; run `38872157394`: 0 blocking findings.
- Diagnostics implemented (`506a8be`): `residual_norm`, `relative_residual`
  (normwise backward error), `NonFiniteSolution` status, aggregate
  `element_equilibrium_error` / `element_moment_equilibrium_error`.
  `StaticSolution::fixed` deliberately left unpopulated — its meaning is
  unspecified.
- Debug / Release / ASan full CTest (run `38872157394`): 30/30 each.
  CLI smoke PASS. Benchmarks: linear scaling in every phase (1,000 →
  10,000 elements: 9.5–11.5×); absolute times vary ~1.5–2× between CI runs.
- Milestone status: **R5 COMPLETE; Phase 3.2 requalification COMPLETE.**

### 2026-10-16 — Phase 4.1: member loads, self-weight, load cases
- Approved conventions (user "devam" on the proposed defaults): global
  member loads per true length; consistent equivalent nodal loads and end
  forces `k*u - FEF`; self-weight via `Material::unit_weight` [kN/m^3] in
  global -Z; load cases from one factorization, combinations by
  superposition; `StaticSolution::fixed` = reactions on restrained DOFs.
- Files: `include/kopruq/loads/Loads.hpp` (new: `NodalLoad` moved here,
  `MemberLoad`, `LoadCase`, `LoadCombination`, equivalent-load quadrature),
  `include/kopruq/solvers/LinearStaticSolver.hpp` (`assembleLoadCase`,
  `recoverAssembled`, `solveCases`; `solve`/`recover` kept as wrappers),
  `include/kopruq/core/Material.hpp` (`unit_weight`),
  `include/kopruq/assembly/DofManager.hpp` (`AssemblyErrorCode::InvalidLoad`),
  `tests/unit/member_load_tests.cpp` (new, 11 tests), `CMakeLists.txt`.
- Debug / Release / ASan full CTest (run `38874706685`): 41/41 each.
  Static analysis: 189 C6326 in tests, 0 blocking. All prior 30 tests
  unchanged and passing; benchmark residuals bit-identical to before
  (nodal-load path arithmetic unchanged).
- Independent references: closed-form EB cantilever (uniform y, uniform z
  with theta_y = -dw/dx, axial, root-to-tip triangular), test-local
  virtual-work integral for a partial trapezoid, fixed-fixed two-element
  beam (qL^4/384EI, qL^2/12, qL^2/24), vector statics for an inclined member
  under global load, superposition and direct solve for combinations.
  All at relative tolerance 1e-9 (combinations 1e-10/1e-12).
- Closes the report's gap "no analytical Iy-plane check" (local-z tests).
- Known limitations: no concentrated member loads or distributed moments;
  reported strain energy is 0.5*u^T*K*u of the nodal field and excludes the
  fixed-end (interior) state when member loads are present.
- Milestone status: **Phase 4.1 COMPLETE.**

### 2026-10-16 — Phase 4.1b: projected global member loads
- User decisions (2026-10-16): global member loads per true length **and**
  per projected length must both be available; projection is onto the
  plane normal to the load direction (horizontal projection for gravity);
  existing element local axes (`reference_y`) are sufficient.
- `MemberLoad::projected` (default false = true length, unchanged): applied
  as `w * L_p / L`, `L_p / L = sqrt(1 - (x . d)^2)`; projected local loads
  rejected (`InvalidLoad`). Files: `include/kopruq/loads/Loads.hpp`,
  `include/kopruq/solvers/LinearStaticSolver.hpp`,
  `tests/unit/projected_load_tests.cpp` (new, 5 tests), `CMakeLists.txt`.
- CI run `38877503974` failed in the Debug job only: post-build
  `gtest_discover_tests` timed out (CMake default 5 s) listing the unchanged
  `kopruq_solver_tests.exe`; Release/ASan passed 46/46. Root cause: cold
  start of a Debug executable on a shared runner. Fix (`cecbe0d`):
  `DISCOVERY_TIMEOUT 60` on every discovery call; no test changes.
- Debug / Release / ASan (run `38878331252`): 46/46 each; static analysis
  0 blocking; benchmark residuals unchanged.
- Milestone status: **Phase 4.1b COMPLETE.**

### 2026-10-16 — Phase 4.2: JSON model I/O, results, CLI solve path
- Approved: Phase 4.2 with `nlohmann-json` (vcpkg, 3.12.0 at baseline),
  used only by the new `kopruq_solver_io` INTERFACE target.
- Files: `include/kopruq/io/{ModelJson,ResultsJson,Sha256,Cli}.hpp` (new),
  `apps/cli/main.cpp`, `schemas/model.schema.json`,
  `schemas/results.schema.json` (full draft 2020-12 schemas replacing the
  placeholders), `examples/{cantilever,portal_frame}.json`,
  `tests/unit/io_tests.cpp` (13 tests), `tests/data/invalid_model.json`,
  `CMakeLists.txt`, `vcpkg.json`, `README.md`.
- Validation is done by the C++ reader (structure, types, ids, references,
  duplicates, value ranges, beam geometry, unknown keys), not by a
  JSON-Schema engine; `ModelJson.SchemaFileMatchesReaderKeys` keeps the
  published schema's property/required sets identical to the reader's.
- CI run `38883546173`: 59/60 in all three configurations, 1 failing test —
  `ModelJson.RejectsInvalidValuesWithPaths`. Root cause (production defect
  in the new reader): nlohmann stores C++-assigned integers as signed, and
  the reader accepted only unsigned ids, so valid ids in programmatically
  built JSON were rejected. Fixed (`24f1c65`) with a regression test.
  (The `b3b6ab0` commit message says 11 I/O tests; there are 12 there, 13
  with the regression test.)
- Debug / Release / ASan (run `38884699955`): 61/61 each; static analysis
  0 blocking (289 C6326 in tests only).
- Verified properties: SHA-256 FIPS vectors; exact model write→read round
  trip; JSON-pipeline solves bit-identical to direct API solves; every
  displacement, reaction and end force serialized exactly; byte-identical
  output for identical input; 30 invalid-input cases + an invalid file
  reported at the expected JSON paths; CLI exit codes 0/1/2/3/4.
- Milestone status: **Phase 4.2 COMPLETE.**

### 2026-10-16 — Phase 4.3: member point loads and distributed moments
- Approved: user "devam" on the recommended option 1.
- `MemberPointLoad` (force [kN] or moment [kNm] at a position fraction,
  local or global axes) in `LoadCase::point`; `MemberLoad::kind`
  (`Force`/`Moment`) for distributed moments [kNm/m]. Moments enter the
  consistent load vector through the shape-function derivatives
  (theta_z = dv/dx, theta_y = -dw/dx) and, for torsion, the linear
  functions. JSON v1 extended additively (`point`, `kind`).
- Files: `include/kopruq/loads/Loads.hpp`,
  `include/kopruq/solvers/LinearStaticSolver.hpp`,
  `include/kopruq/io/ModelJson.hpp`, `schemas/model.schema.json`,
  `tests/unit/point_load_tests.cpp` (new, 15 tests),
  `tests/unit/io_tests.cpp` (schema check covers `point_load`),
  `CMakeLists.txt`.
- CI run `38886496742`: 3/75 failed in every configuration — the point
  moment about z and y and the partial distributed moment tests — only on
  `force_equilibrium_error < 1e-12` (value 1); all displacements and
  reactions matched. Root cause (pre-existing production defect exposed by
  moment-only loading): the metric divided round-off net force by
  round-off force magnitudes. Fix (`62cd955`): force scale also includes
  applied and reaction moments about the model's bounding-box centre
  divided by its diagonal; the metric can only decrease for force-loaded
  cases. Regression test also proves it still detects a perturbed
  displacement field.
- Debug / Release / ASan (run `38887606677`): 76/76 each; static analysis
  0 blocking; benchmark residuals unchanged.
- Known limitation: the per-element `normalized_force_error` has the same
  round-off/round-off form; it stayed < 1e-12 in all current tests but can
  be ill-conditioned for an element carrying only moments. Not changed
  (would alter the documented aggregate definition); recorded for review.
- Local build note: since Phase 4.2 the local `build/` trees no longer
  configure (no local `nlohmann_json`; installing it needs vcpkg compiler
  detection, which ESET blocks), so local compile-only checks are no longer
  possible; CI is the only build.
- Milestone status: **Phase 4.3 COMPLETE.**

### 2026-10-16 — Phase 5.1: beam end releases
- Approved: user "devam" on the recommended option (end releases).
- `Beam3DConnection::releases` (12 member-local flags, default none, so
  existing models/results are unchanged); `kopruq/elements/Releases.hpp`
  does static condensation of K and the member loads' equivalent nodal
  loads; recovery completes the released end's own displacement before
  computing end forces, so k*u - f vanishes there exactly. Unstable
  release combinations (rigid-body motion of the released DOFs with the
  retained ones held, e.g. torsion or axial released at both ends) are
  rejected via the eigenvalues of the diagonally scaled K_rr. JSON v1
  extended additively (`beams[].releases.{start,end}`).
- Files: `include/kopruq/elements/Releases.hpp` (new),
  `include/kopruq/core/Model.hpp`, `include/kopruq/elements/Beam3D.hpp`
  (`UnstableRelease`), `include/kopruq/assembly/MatrixAssembler.hpp`,
  `include/kopruq/solvers/LinearStaticSolver.hpp`,
  `include/kopruq/io/ModelJson.hpp`, `schemas/model.schema.json`,
  `tests/unit/release_tests.cpp` (new, 7 tests),
  `tests/unit/io_tests.cpp` (schema check covers `releases`),
  `CMakeLists.txt`.
- CI run `38892389294`: Release passed 83/83; Debug and ASan failed to
  *build* `apps/cli/main.cpp` with MSVC C1128 ("number of sections
  exceeded object file format limit"). Root cause: the JSON
  reader/writer/CLI header stack is template-heavy enough that Debug and
  RelWithDebInfo(ASan) object files exceed the format limit without
  `/bigobj`; Release's optimizer folds enough sections away to pass
  anyway. `kopruq_solver_tests` already carried `/bigobj` for the same
  reason. Fix (`41aec8a`): `/bigobj` on `kopruq_solver_io` too (covers the
  CLI and all io/point-load/release test executables). Not a numerical or
  test-logic defect.
- Debug / Release / ASan (run `38894147560`): 83/83 each; static analysis
  0 blocking; benchmark residuals unchanged.
- Milestone status: **Phase 5.1 COMPLETE.**

### 2026-10-16 — Phase 4.4: validated model in the C++ API
- Approved: user "devam" on the recommended option (validated model).
- `kopruq::validateModel(model, restraints={}, cases={}, combinations={})`
  (new `include/kopruq/core/Validate.hpp`): reports every issue, not just
  the first, mirroring the JSON reader's semantic checks but addressed by
  index/id (no JSON document here): duplicate/zero ids, invalid
  material/section ranges, missing references, degenerate geometry,
  unstable end-release combinations (checked without solving, via the
  same condensation used at assembly), restraints on unknown nodes, and
  per-load-case/combination checks. `validateModelOrThrow()` throws
  `ModelValidationError` with the full list. Purely additive:
  `solveCases()`/`recover()` are unchanged, still reporting only the
  first problem via whatever exception assembly happens to throw.
- `Restraints` moved from a nested alias inside `LinearStaticSolver` to
  `kopruq::Restraints` in `Model.hpp` (needed as this validator's
  restraints parameter type); `LinearStaticSolver::Restraints` kept as an
  alias to it, so no existing call site changes.
- Files: `include/kopruq/core/Validate.hpp` (new),
  `include/kopruq/core/Model.hpp`,
  `include/kopruq/solvers/LinearStaticSolver.hpp`,
  `tests/unit/validate_tests.cpp` (new, 14 tests), `CMakeLists.txt`.
- Debug / Release / ASan (run `38896544673`): 97/97 each, first attempt;
  static analysis 0 blocking; benchmark residuals unchanged.
- Verified properties: every issue category the JSON reader checks is
  independently flagged with its own path; several simultaneous issues
  are all reported together, not just the first; both example models
  validate clean; the throwing variant carries the complete list.
- Milestone status: **Phase 4.4 COMPLETE.**

### 2026-10-16 — Phase 6.1: modal (free-vibration) analysis
- Approved: user "devam" without picking among the three proposed options
  (5.2 springs/offsets, 6.1 modal, 8 bridge loading); chose 6.1 as the
  lowest-formulation-risk, best-independently-verifiable increment, and
  proceeded per the "decide and report" rule when ambiguity remains after
  offering a clear default.
- `kopruq/elements/MassMatrix.hpp`: consistent 12x12 local mass matrix by
  Gauss quadrature of the same Hermite/linear shape functions and sign
  convention as stiffness/loads; `g_standard = 9.80665` turns
  `Material::unit_weight` into mass density for dynamics only (no new
  public field). `Releases.hpp::condensedMass()`: Guyan mass condensation
  matching the existing stiffness condensation.
  `MatrixAssembler::assembleMass()`. `kopruq/solvers/ModalSolver.hpp`:
  lowest N natural frequencies and mass-normalized mode shapes from the
  free-DOF generalized eigenproblem.
- Files: `include/kopruq/elements/MassMatrix.hpp` (new),
  `include/kopruq/solvers/ModalSolver.hpp` (new),
  `include/kopruq/elements/Releases.hpp`,
  `include/kopruq/assembly/MatrixAssembler.hpp`,
  `tests/unit/modal_tests.cpp` (new, 7 tests), `CMakeLists.txt`.
- CI run `38900022668`: 102/104 passed; 2 failed in every configuration,
  both **test bugs**, not production defects:
  - The eigenproblem residual test computed `K*phi - omega^2*M*phi` over
    the full global vector, including restrained DOFs, where that
    expression equals the support reaction (generally nonzero), not zero;
    it only vanishes on the free DOFs. Fixed by restricting to the free
    rows (found via `MatrixAssembler::classify()`, independent of
    `ModalSolver`'s internals); the mass-orthonormality checks in the same
    test were already correct (zero mode-shape entries at restrained DOFs
    make those terms vanish from `phi^T*M*phi` regardless).
  - The continuum-convergence test used `E*IZ`, but `IY` (0.02) `< IZ`
    (0.04) in that section, so the first (lowest-frequency) mode bends
    about the local y-axis, governed by `IY`; `sqrt(IY/IZ) = 0.707`
    explained essentially all of the observed discrepancy. Fixed to
    `E*IY`.
- Debug / Release / ASan (run `38902536528`): 104/104 each; static
  analysis 0 blocking; benchmark residuals unchanged.
- Known limitations: no JSON/CLI support yet for modal analysis (C++ API
  only); rotary (cross-sectional) inertia about the bending axes is not
  included, standard for slender Euler-Bernoulli beams; `numModes <= 0`
  returns every free-DOF mode, which is O(n^3) via a dense solve and not
  intended for large models.
- Milestone status: **Phase 6.1 COMPLETE.**

### 2026-10-16 — Phase 6.1b: JSON/CLI support for modal analysis
- Approved: user "devam" without picking among four proposed options;
  chose the smallest one (closing the gap just created in 6.1) per the
  "decide and report" rule.
- `kopruq/io/ModalResultsJson.hpp`: new, separate format
  "kopruq-modal-results" v1 (not folded into the static "kopruq-results"
  format, which has a fixed, unrelated key set) --
  `schemas/modal_results.schema.json`. CLI gained a "modal <model.json>
  [-o <out>] [-n <num-modes>]" command; argument parsing and file I/O
  factored into small shared helpers so "solve" and "modal" do not
  duplicate that logic. "solve"'s own behaviour is unchanged.
- Files: `include/kopruq/io/ModalResultsJson.hpp` (new),
  `include/kopruq/io/Cli.hpp`, `schemas/modal_results.schema.json` (new),
  `tests/unit/modal_io_tests.cpp` (new, 6 tests), `CMakeLists.txt`
  (+1 CTest CLI run).
- Debug / Release / ASan (run `38906210839`): 110/110 each, first
  attempt; static analysis 0 blocking; benchmark residuals unchanged.
- Verified properties: every mode-shape component serialized exactly;
  deterministic output; the published schema's top-level and per-mode
  keys match the writer's actual output; CLI exit codes 0/1/2/3/4 for
  the new command, including a default `-n` and an override.
- Milestone status: **Phase 6.1b COMPLETE.**

### 2026-10-17 — GitHub Actions blocker and optimization (Stage 1 of 3)
- After Phase 6.1b, Phase 6.2 (response spectrum analysis) was started on
  `ci/phase6.2-response-spectrum` (code + 6 tests written, one test-side
  spectrum-range bug found and fixed). From 2026-10-16 ~14:00 UTC onward,
  every CI run on that branch failed within ~2 seconds in every job, with
  `runner_id: 0` and zero steps -- no runner was ever assigned, no build or
  test ever ran. A rerun and a fresh run from an empty commit failed
  identically; the pattern persisted into 2026-10-17. Phase 6.2 remains
  **unverified and unmerged** on that branch; Phases up to 6.1b are
  unaffected.
- The user then asked for a GitHub Actions optimization for both KOPRUQ
  (which has no `.github/workflows` at all -- nothing to optimize there)
  and KOPRUQ-SOLVER: move Windows-independent verification to
  `ubuntu-latest`, gate Windows `.exe` production to manual dispatch /
  version tags, add caching, minimal `permissions`, and upload the real
  `.exe` artifacts, without breaking the working build. Plan presented and
  approved; working in 3 approved stages, reporting and waiting for
  approval between each.
- Measured actual cost from this session's own CI history via the GitHub
  API (job `started_at`/`completed_at` across 38 parseable runs): 1,159.6
  wall-clock minutes; at the Windows 2x multiplier, **~2,319 billable
  minutes in under 24 hours** -- exceeding the GitHub Free plan's entire
  2,000-minute monthly allowance in one session. This is the confirmed
  root cause of the runner-provisioning failures.
- Stage 1 (branch `ci/optimize-stage1-ubuntu`, commit `1fce4b4`): new
  `gcc-debug`/`gcc-release`/`gcc-asan` CMake presets; `ci.yml` rewritten
  for `ubuntu-latest` (apt-installed Ninja, Linux vcpkg bootstrap, same
  CLI smoke test/benchmarks against the Linux Release build); added
  caching of the vcpkg tool itself (previously only the built-package
  cache was cached). Source code was checked first and has no Windows/MSVC
  dependency (no `_MSC_VER`/`Windows.h`/`__declspec`; `CMakeLists.txt`
  already has a non-MSVC branch).
- Its first run (`39029543763`) failed identically to the Windows runs:
  all 3 jobs, 2-second lifetime, `runner_id: 0`, zero steps. **This
  confirms the blocker is an account-wide Actions spending/included-minutes
  limit shared across OS types, not a Windows-only quota** -- moving to
  Ubuntu does not by itself restore CI. Consequently, **whether this
  codebase actually compiles under GCC/Clang is still unverified**; Stage
  1's code is pushed but unmerged, pending the user clearing the billing
  limit.
- **Update 2026-10-17, after user confirmation**: runners became available
  again (account-wide blocker cleared). Retried: `gcc-debug` and `gcc-asan`
  passed outright on the first attempt (110/110 CTest each), confirming
  the codebase compiles and runs cleanly under GCC/Clang -- no portability
  defect anywhere. `gcc-release` failed only in the CLI smoke-test step:
  `"$cli" bogus; code=$?` runs under bash's default `-e` (errexit), which
  aborts the step on the command's nonzero exit (2, exactly as designed)
  before `code=$?` is ever reached -- a workflow-script bug carried over
  from porting the Windows PowerShell version (pwsh `run:` steps are not
  errexit by default, so the same pattern worked there). Fixed by isolating
  the intentionally-failing call with `||` instead of `;` (commit
  `f66da33`). Re-run (`39035585549`): all 3 jobs green, 110/110 each.
- Measured real cost (run `39035585549`): `gcc-release` 5.05 min,
  `gcc-debug` 4.80 min, `gcc-asan` 14.83 min wall-clock = 24.68 total
  minutes; at Ubuntu's 1x multiplier, **~24.7 billable minutes per run**,
  vs. the old 4-job Windows run's ~62 billable minutes -- already a ~60%
  cut with only 3 of 4 jobs moved (the 4th, `/analyze`, moves to the rare
  release workflow in Stage 2, which is most of the remaining gap).
- Merged to `main` (fast-forward, `f66da33`).
- Milestone status: **Stage 1 COMPLETE (verified).**

### 2026-10-17 — Stage 2: release.yml for Windows EXE production
- `release.yml` added: the full MSVC debug/release/asan/analyze matrix,
  the CLI smoke test, the benchmark run and vcpkg applocal DLL packaging
  carried over unchanged from the pre-split `ci.yml`; triggers only on
  `workflow_dispatch` or a `v*.*.*` tag push. Added vcpkg tool caching
  (same technique as Stage 1's Linux job).
- `workflow_dispatch` cannot target a workflow that only exists on a
  non-default branch (`POST .../dispatches` returned 404 against the
  feature branch), so verification used a temporary branch-push trigger,
  removed in a follow-up commit once confirmed working -- `main` never
  carried the temporary trigger.
- First run (`39037599675`): all 4 jobs green, 110/110 CTest in each of
  the 3 test jobs, 0 blocking static-analysis findings. The uploaded
  `kopruq-solver-windows-x64` artifact (3.2 MB) turned out to contain
  every GoogleTest executable and `gtest*.dll` alongside the real
  product, because `build/release/*.exe` matches everything the Release
  build produces, not just the CLI and benchmarks.
- Fixed (`42a4280`): narrowed the artifact's `path:` to the three actual
  product binaries (`kopruq_solver_cli.exe`,
  `kopruq_solver_benchmark.exe`, `kopruq_sparse_assembly_benchmark.exe`);
  none need a DLL, since Eigen and nlohmann-json are header-only (the
  DLLs in the first artifact belonged only to the test executables).
  Re-run (`39038848289`): all 4 jobs green again; artifact now 449.8 KB,
  verified (downloaded and listed) to contain exactly those three files.
- Removed the temporary trigger (`e08ab68`) and merged to `main`
  (fast-forward). Confirmed via `GET .../actions/workflows` that
  `release.yml` is now registered and `active` on the default branch
  (dispatchable going forward).
- Milestone status: **Stage 2 COMPLETE (verified).**

### 2026-10-17 — Stage 3: concurrency, permissions review, final comparison
- Added `concurrency: group: ${{ github.workflow }}-${{ github.ref }}` to
  both workflows: `ci.yml` with `cancel-in-progress: true` (concrete,
  observed waste this prevents: this session pushed many commits in quick
  succession to the same branch while iterating, and every superseded run
  kept consuming minutes to completion anyway), `release.yml` with
  `cancel-in-progress: false` (queue instead -- a half-finished release
  silently cancelled by a second dispatch is more surprising than useful,
  and releases are infrequent enough that queuing is never a real wait).
- Permissions reviewed: both workflows already carried only
  `permissions: contents: read`, sufficient for checkout, cmake/ctest,
  cache restore/save and `actions/upload-artifact`; no job needs anything
  broader. No change needed.
- The push+pull_request double-trigger case (a PR opened from a `ci/**`
  branch would fire both events for the same commit) is left as-is: this
  session's workflow never opens PRs against this repo, relying on the
  branch-push trigger alone, and narrowing it would break that pattern for
  no measured benefit. Documented as an accepted, currently-dormant
  tradeoff, not silently changed.
- Verified (run `39045959939`): all 3 `ci.yml` jobs green after adding the
  concurrency block; merged to `main` (fast-forward).
- Milestone status: **Stage 3 COMPLETE (verified). GitHub Actions
  optimization (all 3 stages) COMPLETE.**

## GitHub Actions optimization — final before/after comparison (2026-10-17)
Measured from this session's actual GitHub API data (job
`started_at`/`completed_at`), not estimated.

| | Before (single `ci.yml`, Windows, every push/PR) | After (`ci.yml` Ubuntu + `release.yml` Windows) |
|---|---|---|
| Routine push/PR | 4 jobs (debug/release/asan/analyze), windows-2022, 2x multiplier | 3 jobs (debug/release/asan), ubuntu-latest, 1x multiplier |
| Measured cost per routine run | ~62 billable min (session average, 38 runs, 1,159.6 wall-clock min total) | ~24.7 billable min (run `39035585549`: 24.68 wall-clock min x 1x) |
| Full MSVC matrix (/analyze, real .exe) | Every push/PR | Only `workflow_dispatch` or a `v*.*.*` tag (measured ~103-110 billable min per invocation, runs `39037599675`/`39038848289`) |
| Superseded runs on rapid re-push | Ran to completion anyway | Cancelled immediately (`cancel-in-progress`) |
| `.exe` artifacts | Discarded after each job | Downloadable (`kopruq-solver-windows-x64`, verified exact contents) |
| vcpkg tool (clone + bootstrap compile) | Repeated every run | Skipped on a cache hit |

**This session's actual trigger pattern (46 pushes in <24h) under each system:**
- Before (measured): ~2,319 billable minutes -- exceeded the GitHub Free
  plan's entire 2,000/month allowance in under a day; the confirmed cause
  of the runner-provisioning failures.
- After (projected from measured per-run costs, same 46 pushes + the 2
  release.yml verification runs actually used): 46 x 24.7 + 2 x 106 ~=
  1,136 + 212 = **~1,348 billable minutes, a ~42% cut even under this
  unusually heavy same-day iteration pattern**, and the new
  `cancel-in-progress` behavior (which did not exist in the "before"
  measurement) would reduce it further for any future session that
  pushes corrections in quick succession the way this one repeatedly did.
- For an ordinary usage pattern (routine pushes dominate, releases rare --
  e.g. 100 pushes + 5 releases a month): before ~6,200 billable min/month
  (3x over quota); after ~100x24.7 + 5x106 = 2,470 + 530 = **~3,000
  billable min/month, a ~52% cut**, bringing routine usage much closer to
  the Free plan's 2,000-minute allowance (the remaining gap is primarily
  the number of actual release builds, which the user controls directly
  by choosing when to dispatch/tag).

### 2026-10-17 — Phase 6.2 verified and merged (resumed after the CI work)
- Merged `main` into `ci/phase6.2-response-spectrum` to pick up the new
  Ubuntu/Windows CI split; pushed, and `ci.yml` ran on `ubuntu-latest` for
  the first time this branch's code had ever actually completed a run
  (every earlier attempt failed before reaching any test: first on the
  spectrum-range bug fixed on 2026-10-16, then on the account-wide Actions
  billing blocker).
- Result: 115/116 passed; `ResponseSpectrum.MassParticipationSumsToOne-
  WithAllModes` failed identically in all 3 jobs (deterministic, not a
  sanitizer/platform issue). Root cause: a genuine formulation gap, not a
  portability one -- `iota` was 1 at every translational DOF *including
  restrained ones*, so `Gamma_i = phi_i^T*M*iota` picked up a mass-
  coupling term through the restrained DOFs that no finite set of
  free-DOF modes can ever fully capture; `mass_participation_ratio`
  summed to ~0.9265, not 1.0, even with every free-DOF mode included.
- Fixed (`b207adf`): `iota` is now 1 only at free translational DOFs
  matching `direction` (checked via DOF index mod 6), 0 at rotations and
  restrained DOFs -- matching the modal participating-mass convention
  most structural software reports (SAP2000, ETABS). `total_mass` is now
  the free DOFs' own mass in that direction, not the whole structure's
  physical mass (the restrained supports' mass share is excluded by
  construction, which is the point of the fix). Updated
  `ResponseSpectrumSolver.hpp`'s formulation comment accordingly.
- Test-side (`5807f51`): `TotalMassMatchesIndependentPhysicalTotal`
  renamed to `TotalMassMatchesIndependentFreeDofReduction` and rewritten
  to independently assemble the global mass matrix, drop the restrained
  node, and compute `iota_free^T*Mff*iota_free` (asserted strictly less
  than the full physical mass); `SingleElementModeMatchesIndependent-
  DenseComputation`'s `iota` corrected to the free node only.
- Re-run (`39051080335`): **116/116 passed in all 3 jobs** (`gcc-debug`,
  `gcc-release`, `gcc-asan`). Merged to `main` (fast-forward).
- Note on verification going forward: per the new CI architecture (Stages
  1-3), Ubuntu `ci.yml` is now the routine verification evidence for every
  change, including this one; the full Windows MSVC matrix in
  `release.yml` is reserved for actual release events (manual dispatch or
  a version tag), not run per merge, by design -- this merge intentionally
  does not have a corresponding Windows run, consistent with the
  cost-reduction goal just completed.
- Milestone status: **Phase 6.2 COMPLETE (verified).**

### 2026-10-17 — Phase 6.2b: JSON/CLI support for response spectrum analysis
- Approved: user "devam" without picking among five proposed options;
  chose the smallest one (closing the gap 6.2 just created, same pattern
  as 6.1 -> 6.1b) per the "decide and report" rule.
- Model JSON v1 gains an optional, additive "response_spectra" array:
  named requests (direction, optional num_modes, a spectrum of
  {period, sa} points) round-tripping through the same ResponseSpectrumInput
  the C++ API takes. New, separate "kopruq-response-spectrum-results" v1
  format (not folded into kopruq-results or kopruq-modal-results): per-mode
  summary + SRSS-combined peak response, omitting each mode's full
  per-node/per-element AnalysisResults (C++ API only) to keep the document
  a practical size. New CLI "spectrum <model.json> [-o <out>]" command.
  examples/portal_frame.json gained a representative "EQX" case.
- Files: `include/kopruq/io/ModelJson.hpp`,
  `include/kopruq/io/ResponseSpectrumResultsJson.hpp` (new),
  `include/kopruq/io/Cli.hpp`, `schemas/model.schema.json`,
  `schemas/response_spectrum_results.schema.json` (new),
  `examples/portal_frame.json`, `README.md`,
  `tests/unit/spectrum_io_tests.cpp` (new, 9 tests),
  `tests/unit/io_tests.cpp` (schema check covers the 2 new definitions),
  `CMakeLists.txt` (+1 CTest CLI run).
- `ubuntu-latest` (run `39055492002`): all 3 jobs green on the first
  attempt, 125/125 each. Merged to `main` (fast-forward).
- Milestone status: **Phase 6.2b COMPLETE (verified).**

### 2026-10-17 — Scope change: Phase 6.3, 7, 8 cancelled (user decision)
- User instruction (verbatim intent): cancel Phase 7, Phase 8, and Phase
  6.3 in `docs/SOLVER_ROADMAP.md`; these will not be implemented. Revise
  the plan accordingly and rewrite the roadmap.
- Cancelled, removed from the roadmap (not deferred):
  - **Phase 6.3** -- linear buckling / P-Delta (geometric stiffness).
  - **Phase 7** -- time-dependent and nonlinear analysis (time-history
    integration/damping, geometric/material nonlinearity, incremental-
    iterative convergence controls).
  - **Phase 8** -- bridge engineering capabilities (construction stages,
    prestressing/creep/shrinkage/temperature, bearings/soil springs,
    moving loads/influence lines, bridge-specific load combinations).
- Nothing already implemented depended on these phases (6.1/6.2/6.2b are
  unaffected; no code references buckling, time-history, or any
  bridge-specific module), so this is a pure scope reduction with no code
  impact.
- `docs/SOLVER_ROADMAP.md` rewritten: the former Phase 9 (high-performance
  production solver), Phase 10 (KOPRUQ integration) and Phase 11
  (AI-assisted optimization) are renumbered to Phase 7, 8 and 9
  respectively, since nothing in them depended on the cancelled phases.
  A "Scope change" note at the top of the roadmap records the decision
  and points here for the full rationale.
- The `Product objective` paragraph ("initially optimized for bridges")
  was left unchanged -- the user cancelled specific roadmap phases, not
  the mission statement; flag for a future explicit decision if that
  statement should also be revised given Phase 8's cancellation.
- Milestone status: **Scope change recorded and applied; no code
  changed.**

### 2026-10-17/18 — Phase 5 completion: truss, rigid offsets, spring/link elements
User instruction (verbatim intent): complete Phase 5, then Phase 6, without
asking for approval between them; stop before Phase 7.

- **5.2 truss elements** (`include/kopruq/elements/Truss3D.hpp`): a
  pin-ended 3D truss implemented as a Beam3D with every rotational DOF
  released, reusing 5.1's static condensation rather than a new
  stiffness/mass/load/recovery path. First attempt released RX at both
  ends; CI (run `39060519026`) caught `Beam3DErrorCode::UnstableRelease`
  -- releasing torsion at both ends creates a genuine, separate zero-energy
  spin mechanism (decoupled from everything else), which Releases.hpp's
  stability check correctly rejects. Fixed by releasing RX at node2 only
  (commit `a1e8212`): releasing either single end already drives the
  retained torsional stiffness to exactly zero (the torsion block is
  rank-1), so the fix changes nothing mechanically, only avoids the
  both-ends-free degeneracy. Independent verification: dense comparison to
  the classical EA/L*c c^T truss stiffness formula; a statically
  determinate two-bar A-frame matched against hand statics (method of
  joints) and the unit-load/virtual-work theorem, cross-checked against
  elongation-derived axial force.
- **5.3 rigid end offsets** (`include/kopruq/elements/Offsets.hpp`): a
  rigid-body kinematic transform G (u_point = u_node + theta_node x e)
  composed around the existing offset-point stiffness/mass/loads
  (K_node = G^T K_point G); zero offsets are an exact no-op, verified by a
  bit-identical regression test against the pre-existing beam path.
  Independent verification: rigid-body kinematics of G itself; G^T K G
  composed in the test from Beam3D::globalStiffness (an already-verified
  primitive) vs. the production-assembled matrix; equilibrium/energy
  residuals for a loaded, offset cantilever.
- **5.4 spring/link elements** (`include/kopruq/elements/SpringLink.hpp`):
  a two-node, six-independent-DOF discrete spring (no coupling between
  DOFs, unlike a beam), covering bearings/isolators/soil springs. CI (run
  `39068274432`) caught a genuine formulation defect: with non-coincident
  nodes, the decoupled per-DOF stiffness has no lever-arm coupling to the
  node separation, so it lacks a rigid-body null space under rotation --
  `Spring.TiltedLoadedSpringSatisfiesEquilibrium`'s moment_equilibrium_error
  was 0.86, not near-zero. Fixed (commit `189df29`) by requiring node1/node2
  to be at the same location (`checkSpringNodesCoincident`, enforced in
  assembly, `Validate.hpp` and the JSON reader) rather than generalizing the
  element -- the documented primary use (a single-point bearing/isolator)
  was always coincident-node, so nothing that ever worked regresses. A
  second CI failure (run `39069440192`) was a pure editing mistake --
  the new function landed outside its namespace/includes -- fixed in commit
  `7a6fb83` with no logic change.
- Independently derived expected results used statics, virtual-work and
  plain rigid-body kinematics throughout; none were computed by calling
  the production code under test. Both CI-caught issues were genuine
  formulation/mechanism defects this session's own equilibrium-residual
  tests were specifically designed to catch, not test-authoring mistakes --
  recorded here rather than glossed over.
- Final verification: CI run `39069625215`, commits `2228863`, `a1e8212`,
  `9126760`, `189df29`, `7a6fb83`. Debug/Release/ASan: **149/149 PASS**
  (up from 133 before this session's work; 16 new tests across
  `truss_tests.cpp`, `truss_io_tests.cpp`, `offset_tests.cpp`,
  `spring_tests.cpp`, `spring_io_tests.cpp`).
- Deferred, not attempted: general multi-point rigid links/constraints
  (different mechanism from 5.3's two-node offset; needs DofManager-level
  dependent-DOF elimination or Lagrange multipliers); the Timoshenko beam
  option (no stated requirement; would touch the most heavily-verified
  core element, Beam3D.hpp).
- Milestone status: **Phase 5 COMPLETE as scoped. Phase 6 already complete
  (6.1, 6.1b, 6.2, 6.2b; 6.3 cancelled) -- confirmed, no further action
  needed there.** Stopping before Phase 7 per the user's explicit
  instruction.

### 2026-10-18 — Phase 5 completion: rigid links (5.5) and Timoshenko beam (5.6)
User instruction (verbatim intent): complete the items deferred from
Phase 5 -- general rigid links and the Timoshenko beam option -- in full,
after reviewing the prior progress report.

- **5.6 Timoshenko beam**: `Section::Asy`/`Asz` (effective shear area,
  default 0 = rigid in shear). `Beam3D::localStiffness` now computes
  ay/by/cy/dy (and az/bz/cz/dz) with the standard (1+Phi) correction,
  Phi = 12EI/(GAs L^2); every call site building `Beam3DProperties` from a
  `Section` updated to pass Asy/Asz through (otherwise the correction
  would silently never apply). Mass matrix intentionally left
  Euler-Bernoulli (documented limitation, not an oversight).
  Independent verification: Asy=Asz=0 reproduces the plain
  Euler-Bernoulli formulas bit-for-bit; a cantilever tip deflection/
  rotation under a transverse point load matches the closed-form
  Timoshenko solution w=PL^3/(3EI)+PL/(GAs), theta=PL^2/(2EI) (theta
  independent of shear, as theory predicts). **Test-authoring bug caught
  by CI** (run `39103459292`): the point load's `position` was 0.0 (node1,
  the fixed end) instead of 1.0 (node2, the free tip), so every case
  measured a zero-displacement no-op; fixed in commit `a2f5290`, no
  production code touched.
- **5.5 general rigid links**: `RigidLink.hpp`, a slave node's 6 DOFs
  entirely determined by a master's via rigid-body kinematics (one master,
  many slaves possible), implemented as a DOF-elimination layer wrapped
  around the existing, unmodified assembled K/F/U
  (K_reduced=T^T K T, etc.). To keep the overwhelmingly common
  no-rigid-links case at zero risk, `LinearStaticSolver::solveCases` has
  an early branch to a separate `solveCasesWithRigidLinks` function
  (a parallel implementation, not conditionals threaded through the
  original) when `m.rigid_links` is non-empty. `ModalSolver`/
  `ResponseSpectrumSolver` explicitly reject models with rigid links
  (not yet supported there) rather than silently ignoring the constraint.
  Independent verification: the reduction matrix's kinematics checked
  directly against the cross-product definition; a cantilever with a
  rigid arm past its tip, loaded at the arm's end, checked against the
  closed-form cantilever-under-combined-force-and-moment solution (the
  equivalent load from elementary statics) and, separately, against the
  rigid-body kinematic relationship itself.
- Final verification: CI run `39104401638`, commits `edc0e6d`, `a2f5290`,
  `18b6a70`. Debug/Release/ASan: **167/167 PASS** (up from 149; 18 new
  tests across `timoshenko_tests.cpp`, `timoshenko_io_tests.cpp`,
  `rigid_link_tests.cpp`, `rigid_link_io_tests.cpp`).
- Milestone status: **Phase 5 COMPLETE IN FULL (5.1-5.6), no remaining
  deferred items.** Stopping again before Phase 7 per the still-standing
  instruction from the 2026-10-17 approval checkpoint.

### 2026-10-18 — Phase 7A: MIDAS NX JSON import audit (first execution, audit only)
User instruction (verbatim intent): make KOPRUQ-SOLVER able to import a real MIDAS Civil NX JSON
export, analyze it with KOPRUQ-SOLVER's own solver, and export JSON results, independently
verified against analytical references first and MIDAS's own results where available. Added
Phases 7A-7F to the roadmap, kept Phase 8/9 deferred as before. First execution explicitly
scoped to audit + roadmap update only, ending at a mandatory approval checkpoint before any
Phase 7B implementation.

- Inspected `xmidas/deneme1.json` directly (PowerShell `ConvertFrom-Json`; no Python available
  in this environment). Full findings: `docs/MIDAS_JSON_DATA_AUDIT.md`. Summary: 106 nodes, 94
  beam elements, 2 materials, 5 sections, 9 fully-fixed supports, 4 elastic links, 5 rigid links,
  5 load cases (self-weight, 3 superimposed dead load stages, 1 temperature), 40 member loads, 40
  element-temperature records, 1 modal case (Lanczos/20 modes), 2 response-spectrum cases
  (Eurocode 8 elastic spectrum, CQC). This is a full MIDAS database dump (150+ categories), not a
  minimal structural subset -- most categories are empty for this model.
- Official MIDAS documentation (`support.midasuser.com` API manual and Export-to-JSON pages)
  returned HTTP 403 (authenticated support portal) -- not used, per the task's own fallback
  instruction. One public page (concrete material modulus formula) was reachable via search and
  corroborates finding (1) below.
- Five real gaps found, in blocking-severity order: (1) material elastic modulus/Poisson/unit
  weight are not in the export -- only a design-code grade name (`C35/45` etc.) and
  characteristic strength; MIDAS derives `E` internally via a code formula. (2) section area/
  Iy/Iz/J are not in the export -- only a shape code (`SB`,`B`,`SR`) and raw dimensions; needs a
  section-shape property library. (3) the sample's 4 elastic links connect **non-coincident**
  nodes, which Phase 5.4's `SpringLink` deliberately rejects (a real formulation limit found and
  enforced by CI during that phase, not an oversight) -- this specific model cannot be imported
  onto `SpringLink` as it stands. (4) the sample's rigid links coexist with modal/response-
  spectrum requests, but `ModalSolver`/`ResponseSpectrumSolver` (Phase 5.5) explicitly reject any
  model with rigid links. (5) element-temperature loads (`ETMP`/case `TU`) have no implementation
  in any phase. `docs/SOLVER_CAPABILITY_MATRIX.md` lays out every capability the sample touches
  against current solver status.
- MIDAS's own modal frequencies (all 20 modes) are readable directly from
  `xmidas/deneme1_Eigenvalue.out` (plain text) -- useful, ready-made independent reference data
  for a future Phase 7E comparison. No numeric static/spectrum results were found in readable
  form; those live in binary `.rst`/`.mec`/`.nfxp` files, not parsed in this audit.
- `docs/SOLVER_ROADMAP.md` updated with Phases 7A (COMPLETE, this execution) through 7F (not
  started), inserted after Phase 7 (production solver, also not started) and before Phase 8/9
  (still deferred, unchanged).
- Milestone status: **Phase 7A COMPLETE. No code changed. Mandatory approval checkpoint: Phase
  7B (the actual importer) requires explicit user approval before starting**, per the user's own
  instruction for this execution.

### 2026-10-19 — Phase 7B implementation (CI-blocked) + Phase 7S/7M/7T roadmap addition
Phase 7B (MIDAS JSON importer) was implemented per the user's "devam" approval following the
Phase 7A report: `MidasMaterialLibrary.hpp`, `MidasSectionLibrary.hpp`, `MidasOrientation.hpp`
(the local-axis convention, confirmed against MIDAS's own manual text found via web search after
WebFetch to midasuser.com returned HTTP 403 throughout), and `MidasJsonImporter.hpp`, plus
`xmidas/deneme1.json` added to the repo so CI can read it. Two explicit clarifying questions were
asked and answered before implementing: missing material Poisson/unit-weight -> use disclosed
standard values rather than fail; local axis convention -> user asked to confirm via MIDAS itself,
then supplied a documentation link, whose content was retrieved via web search (not WebFetch,
which stayed blocked) and matched the general convention already drafted.

GitHub Actions has been returning "no runner assigned" (empty steps, `runner_id:0`, 2-3 second
failures) since partway through 2026-10-18, continuing into 2026-10-19 -- the same pattern as an
earlier, account-wide billing/quota episode in this project's history, reported to the user twice;
not resolved as of this entry. **No commit since `0823f8f` has been compiled or run anywhere.**
Import counts (106 nodes, 94 beams, 2 materials, 5 sections, 9 supports, 7 individual rigid
links, 4 load cases, 70 member loads) were cross-checked independently via PowerShell
`ConvertFrom-Json` on the raw source file, but that only confirms the *expected* values the C++
test asserts against, not that the C++ itself is correct.

Immediately after, the user opened a second, separate instruction: extend the roadmap (audit/
update only, no implementation) with three new general-purpose capabilities -- Phase 7S
(parametric/general section property engine), Phase 7M (Eurocode material library by grade
name), Phase 7T (uniform/gradient temperature analysis) -- generalizing what 7B's MIDAS-specific
helpers currently do ad hoc, plus adding thermal/temperature support KOPRUQ-SOLVER has never had
in any phase. `docs/SOLVER_ROADMAP.md` updated (7S/7M/7T inserted between 7B and 7C, with scope,
required properties, section families subject to per-family approval, and explicit "not yet
implemented" status); nothing implemented. Milestone status: **roadmap updated; mandatory
approval checkpoint, per this execution's own explicit instruction.**

### 2026-10-19 — GitHub Actions blocker resolved; Phase 7B/7M/7S(1) verified in one run
User increased the GitHub Actions quota on their account; the "no runner assigned" blocker
present since 2026-10-18 cleared immediately (run `39223638115` got a real runner on the first
rerun attempt after the increase). All six commits accumulated while blocked
(`0823f8f`,`281b2b2`,`bd5ebbe`,`64ce16f`(docs),`557d375`,`f10dca0`) were compiled and tested for
the first time in that run.

Result: only ONE real defect, in Phase 7S's brand-new polygon section engine -- the centroid
accumulator divided by 12 instead of 6 (a transcription slip next to the correctly-12/24-divided
second-moment accumulators), giving centroids exactly half their correct value. Invisible in
every test using an origin-centered section (0/12 == 0/6 == 0); caught only by
`OffsetRectangleCentroidAndParallelAxisAreConsistent`, a test written specifically to check
translation invariance. Fixed in commit `59b0035`, re-verified: CI run `39227258173`,
**199/199 Debug/Release/ASan, all green**.

Everything else -- Phase 7B's full MIDAS importer (including orientation conversion and the
end-to-end real-sample test) and Phase 7M's Eurocode material library -- passed on the very
first compile, including the test that matters most: the real `xmidas/deneme1.json` bridge
model imports, passes `validateModel`, and **solves successfully** through
`LinearStaticSolver::solveCases` with no exception and `status.success == true`. This is the
first time this session's MIDAS import work has been verified by anything other than hand
analysis and independent PowerShell cross-checks.

Milestone status: **Phase 7B COMPLETE. Phase 7M COMPLETE. Phase 7S increment 1 COMPLETE
(general polygon engine + hollow circle; I/H/T/channel/angle/girder sections still explicitly
deferred, each needing its own approval). Phase 7T (temperature loads) is next.**

### 2026-10-19 — Phase 7T implementation and verification (2 issues caught and fixed)
`Material::thermal_expansion`, `MemberThermalLoad`/`LoadCase::thermal`, and
`include/kopruq/loads/ThermalLoads.hpp` (uniform + gradient equivalent nodal loads, derived via
the same virtual-work integral already used throughout this project) implemented and wired into
`LinearStaticSolver::assembleLoadCase` via the existing `accumulate()`/`frameOf()` machinery.
JSON I/O, schema and `Validate.hpp` updated. First CI run (`39233490979`) caught a test-design
flaw in `ThermalLoads.FreeUniformExpansionMatchesAlphaDeltaTL`: `force_equilibrium_error` is a
ratio that reads exactly 1.0 for ANY nonzero reaction when the applied-load resultant is exactly
zero (true by design for a thermal load, which has no real external resultant) -- fixed
(`9c53fd1`) by checking the raw `force_balance_norm` instead. Second CI run (`39235802175`)
caught a real, pre-existing solver robustness gap:
`ThermalLoads.FullyRestrainedUniformGivesEAAlphaDeltaT` and
`...FullyRestrainedGradientGivesEIKappaMoment` both restrain every DOF of a 2-node model (by
design, to measure the fixed-end thermal force/moment directly) -- a legitimate, trivially-solved
model, but `SimplicialLDLT::compute()` on the resulting 0x0 `Kff` aborted the process in the
Debug build specifically (Release tolerated it, masking the issue there). Fixed (`1dd5323`) in
both `solveCases` and `solveCasesWithRigidLinks`: a model with zero free DOFs now skips the
factorization/solve entirely rather than relying on Eigen's undefined-across-configurations
behavior for a 0x0 matrix.

Final verification: CI run `39237884916`, commits `bd0a079`, `9c53fd1`, `1dd5323`.
**Debug/Release/ASan: 215/215 PASS** (up from 199; 16 new tests across `thermal_load_tests.cpp`,
`thermal_io_tests.cpp`). This completes the 7M -> 7S -> 7T implementation order in full.

### 2026-10-19 — MIDAS ETMP wired to Phase 7T thermal loads
Per the user's explicit "2. MADDEYE DEVAM EDELİM" instruction, picked item 2 from the previous
report's candidate list: wire Phase 7B's MIDAS importer to Phase 7T instead of rejecting
`TU`/`ETMP`. Inspected the real export's `ETMP` records directly (PowerShell `ConvertFrom-Json`):
all 40 are the simple `{ID, LCNAME, GROUP_NAME, TEMP}` shape, `TEMP=40.0` for every one -- a
single uniform value, confirmed (via a web search of MIDAS's own documentation, since
support.midasuser.com itself returns HTTP 403) to be a SEPARATE category (`GTMP`) from gradient
temperature, whose `TZ`/`TY` fields and unconfirmed T1/T2 top-fiber sign convention are
deliberately NOT mapped -- `GTMP`/`BTMP` are recognized and rejected explicitly instead, with 0
records in this sample so that path is untested against real data (covered by three new tests
against a minimal hand-built MIDAS-shaped JSON snippet instead).

CI (run `39245168495`) caught a compile error (not a logic bug): a plain `R"(...)"` raw string
literal was terminated early by the `)"` sequence embedded in `"EN04(RC)"` inside the test file's
own synthetic JSON content -- fixed (`b43f605`) with a custom delimiter (`R"json(...)json"`).

Final verification: CI run `39247142910`, commits `2b19d2a`, `b43f605`. **Debug/Release/ASan:
219/219 PASS** (up from 215; 4 new tests). The real `xmidas/deneme1.json` model, now with its
`TU` thermal case actually active (not a zero no-op -- `MidasMaterialLibrary.hpp` also assigns
the EN1992-1-1 S3.1.3(5) thermal expansion coefficient), still **solves successfully**.

## GUI-0 — Qt 6 Verification GUI repository audit (2026-10-20)
Per the user's "QT6 VERIFICATION GUI" task spec, GUI-0 (audit only, no implementation) is
**COMPLETE**. Full findings recorded in `docs/SOLVER_ROADMAP.md`'s new "QT6 VERIFICATION GUI --
STANDALONE MIDAS NX VALIDATION" section (purpose/scope/architecture/structure/UI layout/GUI-0..7
milestone table/7C-7D dependency/Excel requirements/comparison rules/tolerances/acceptance/
deployment/risks/future work/tracking). Key findings, condensed:
- Phase 7C/7D (CLI `--format midas-nx-json`, results export) are complete and CI-verified --
  GUI-3 can invoke the real CLI exactly as documented, no backend changes needed.
- **No Excel/XLSX handling exists anywhere in the repo today** (grep returned zero matches); a new
  dependency is required in GUI-4. The reference `.xls` files already in `xmidas/` are legacy
  BIFF/OLE2, not `.xlsx` -- this must be confirmed/handled before picking a library.
- **No Qt6 anywhere**: not in `vcpkg.json`, not installed locally (no `qmake` found), no CI job
  builds it. `ci.yml` (every push) is Ubuntu-only with no Qt6; `release.yml` (Windows, MSVC, the
  natural GUI build target) is manual-dispatch/tag-only, specifically to avoid repeating an
  earlier Actions-minute exhaustion incident this project already hit once. Local build
  verification is also not currently possible in this dev environment (`cmake` not on `PATH`,
  confirmed this session).
- **Central open risk, flagged rather than decided unilaterally**: since neither routine CI path
  nor the local environment can currently build/launch a Qt6 app, GUI-1's own acceptance criteria
  ("application compiles", "application opens") need an explicit decision on verification strategy
  before GUI-1 starts (see roadmap section 14 for the three candidate approaches and their
  tradeoffs).
- Element-type information needed to separate beam-force rows from elastic-link rows in GUI-5's
  comparison tables is not present in `results.json` itself (element results are keyed only by id,
  no type tag) -- the matcher will need to join against the *input* `ModelInput`, which does carry
  element types.
No code was written in GUI-0, per the task's explicit instruction.

## GUI-1 — main window, four-area dark layout (2026-10-20)
**COMPLETE AND VERIFIED.** User chose option (b) for the GUI-0 verification-strategy question:
manual `release.yml` (Windows) dispatch after each GUI milestone. Implemented
`apps/verification-gui/` (`MainWindow` + `FileInputPanel`/`AnalysisControlPanel`/
`ExecutionLogPanel`/`ResultsComparisonPanel`, dark QSS) exactly per the approved reference layout,
no business logic (buttons disabled, no QProcess/dialog/comparison wiring -- GUI-2 onward). Build
is opt-in and fully isolated from the every-push Ubuntu `ci.yml`: a new `"gui"` vcpkg manifest
feature (`qtbase`) is only fetched when `-DVCPKG_MANIFEST_FEATURES=gui` is passed, which only the
new `ninja-gui-release` preset does.

Three manually-dispatched `release.yml` runs were needed to reach green, each catching a real
defect (see `docs/SOLVER_ROADMAP.md`'s "GUI-1 verification" entry for full detail): an MSVC-only
`auto*`-binding-a-lambda compile error (`6d7bc82`), a missing `testPresets` entry for the new
preset (`9c74ba9`), then **PASS** on run `39406148285` -- all five Windows presets green,
**220/220 tests**, and the GUI smoke test confirmed `kopruq_verification_gui.exe` actually
launches (with `windeployqt`-deployed Qt6 runtime/platform-plugin), stays alive 5s, and closes
cleanly. No existing regression test was touched.

## GUI-2 — file selection, extension/existence validation, QSettings (2026-10-20)
**COMPLETE AND VERIFIED, first dispatch.** `FileInputPanel` now wires a real `QFileDialog` per
row, validates by extension (`.json` for the MIDAS model; `.xlsx`/`.xls` for the four reference
rows -- widened to include `.xls` since the real MIDAS exports already in `xmidas/` are legacy
BIFF, not `.xlsx`, per GUI-0's audit) and `QFileInfo::exists()`, producing
NotSelected/Selected/Invalid/Unsupported (deliberately never "Valid" -- that claim belongs to the
"Modeli Doğrula" action once GUI-3 wires the real CLI, not to a file-selection check). Every path
is persisted via `QSettings` and restored-and-revalidated on next launch, so a moved/deleted file
correctly shows Invalid rather than silently reappearing as selected. The four Excel rows remain
individually optional; only the MIDAS JSON row is required (`hasRequiredModel()`). No action
button is wired yet -- still GUI-3's scope.

Verification: `release.yml` run `39422159633`, all five Windows presets `success`, **220/220
tests**, GUI smoke test confirmed a clean 5s launch/close (exercising `FileInputPanel`'s full
construction including `QSettings` restore on a runner with no prior settings). Honestly noted
limitation: the interactive file-dialog flow itself and cross-restart persistence are not
exercised by an unattended CI job (no one to click a dialog) -- confirmed by code review of the
validation/persistence logic instead; see `docs/SOLVER_ROADMAP.md`'s "GUI-2 verification" entry.

## GUI-3 — solver execution via QProcess, real CLI (2026-10-20)
**COMPLETE AND VERIFIED, first dispatch.** New `SolverRunner` (async `QProcess` wrapper):
`locateCli()` searches plausible developer-build locations next to the GUI exe then `PATH`,
returning empty (never guessed) if not found. `run()` always invokes `solve <model> -o <output>
--format midas-nx-json` -- exactly `Cli.hpp`'s documented contract, no invented flags, since this
GUI is specifically a MIDAS verification tool. "Modeli Doğrula" and "Solver'ı Çalıştır" both run
that identical command (the CLI has no validate-only mode) and differ only in how `MainWindow`
interprets the exit code: Validate reports only invalid/valid (exit 3 vs. not), never claiming a
solve succeeded; Run Solver reports the full 0 (success)/1 (results written but failed)/other
contract. Progress bar is indeterminate while running (no granular CLI progress exists) and only
reaches 100% on real success -- never a fabricated percentage. `closeEvent` terminates/kills an
in-flight process rather than orphaning it.

Verification: `release.yml` run `39428529324`, all five Windows presets `success`, **220/220
tests**, GUI smoke test confirmed a clean 5s launch/close (now also constructing `SolverRunner`
and wiring its signals without a crash). Honestly noted limitation, same as GUI-2: an actual CLI
invocation end to end (process launch, stdout capture, exit-code handling, results.json
discovery) is not exercised by unattended CI, since nothing selects a model or clicks Run there
-- confirmed by code review against `Cli.hpp` instead; a real local run against
`xmidas/deneme1.json` is the natural manual confirmation once GUI-4/5 exist to make results
useful.

## GUI-4 — MIDAS Excel reference parsing (2026-10-20)
**COMPLETE AND VERIFIED, first dispatch.** Format decision: the real samples in `xmidas/` are
legacy `.xls` (BIFF/OLE2), but no vcpkg library reads that format. Asked the user to choose
between vendoring `libxls` (no vcpkg port, higher engineering/CI risk with no local build to
verify against), `.xlsx`-only support, or re-exporting the samples -- user chose **re-export as
`.xlsx`**. New `verification/ExcelReferenceReader.{h,cpp}` (Qt-independent) reads via `xlnt`
(vcpkg, added to the "gui" manifest feature alongside `qtbase`): every field is located **by
header text**, never a hardcoded column index (the four real export types have different column
counts confirmed against the real samples: Beam Force 23, Elastic Link 19, Displacements/
Reaction 15), and the header row itself is located by content, not a fixed row index (MIDAS's
exports have a title row above it). `.xls` files are rejected via their real OLE2 signature
(never trusted from the extension) with an explicit diagnostic, both in the reader and in
`FileInputPanel` (GUI-2's Excel rows now accept `.xlsx` only). Not wired into "Sonuçları
Karşılaştır" yet -- that's GUI-5.

Verification: `release.yml` run `39435756925` (first dispatch after GUI-4 2026-10-21 status-check
request confirmed it completed clean): all five Windows presets `success` (`xlnt` built from
vcpkg source for the first time, ~50 min, now cached). **226/226 tests** (6 new
`ExcelReferenceReader` tests, built with self-contained `.xlsx` fixtures via `xlnt` itself, no
external fixture file). GUI smoke test confirmed another clean 5s launch/close. Windows GUI
executable artifact uploaded successfully (40,572 bytes). No existing test modified or removed.

## GUI-5 — comparison engine, PASS/FAIL/N-A/UNSUPPORTED, real tables (2026-10-20)
**COMPLETE AND VERIFIED, first dispatch.** New `ComparisonEngine` (combined abs/rel criterion,
independent documented absolute-tolerance floor per physical quantity -- displacement m, rotation
rad, force kN, moment kNm; the approved UI's single Mutlak/Bağıl Tolerans pair maps to a
dimensionless scale factor + the legitimately-shared relative tolerance) and `ResultMatcher`
(matches `results.json` against a `ReferenceTable` by original MIDAS id + load case name, both
preserved unchanged by Phase 7B's importer; Beam Force reads the `Part` column's `I[...]`/
`J[...]` prefix to pick the right half of `local_end_forces`; elastic links always report
`Unsupported`, since Phase 7B rejects every MIDAS `ELNK` record -- a real formulation gap).
`MainWindow::onCompare()` wires "Sonuçları Karşılaştır" to this for real: reads the last solve's
`results.json`, reads whichever Excel files are actually selected (each independently optional),
populates all five result tables (Özet's tiles + category table, Deplasmanlar, Mesnet
Tepkimeleri, Kiriş İç Kuvvetleri, Elastik Bağlantılar) plus Diagnostik with every diagnostic.

Verification: `release.yml` run `39468392733`, all five Windows presets `success`. **237/237
tests** (11 new: 7 `ComparisonEngine` + 4 `ResultMatcher`, hand-built fixtures, no Excel I/O
needed). GUI smoke test confirmed another clean 5s launch/close. No existing test modified.

**Known open question, not resolved here:** MIDAS's beam-force sign convention (confirmed via
WebSearch, never independently cross-checked against a solved result) is explicitly Phase 7E's
job, still not started -- if the real model shows systematic beam-force FAILs once compared, that
is this milestone correctly surfacing a real question, not a GUI-5 defect.

**Confirmed against real data (2026-11-16, GUI V2.1 session):** ran the GUI's actual "Sonuçları
Karşılaştır" against model 05 (Cerceve-1)'s genuine (user re-exported, byte-signature-verified
real OOXML) `.xlsx` reference files -- 900 total, 757 PASS, 143 FAIL, 0 N/A. Reproduced the exact
same match independently in Python (reading the same `.xlsx` cells and `results.json` fields the
production `ResultMatcher` reads) to attribute every FAIL: **Deplasmanlar 300/300 PASS, Mesnet
Tepkimeleri 24/24 PASS, all 143 FAILs are in Kiriş İç Kuvvetleri (Beam Force)**, and every one of
those 143 is a magnitude-exact, sign-opposite pair (e.g. element 31, I end: MIDAS Axial=-11750,
solver=+11750; Shear-z=-4425.2785 vs +4425.278698; element 31, J end: MIDAS Moment-y=-4037.32925
vs +4037.3292658) -- never a real magnitude discrepancy. This is exactly the above open question
surfacing on real data, not a GUI/ComparisonEngine/ResultMatcher defect: per explicit user
decision this session, left unresolved (no sign-convention flip added) until Phase 7E
independently verifies MIDAS's actual I/J-end axial/shear/moment sign convention; `displacementAbsTol`
and reaction comparisons needed no changes and are genuinely verified against this model's real
reference data.

## GUI-6 — Excel verification report export (2026-10-21)
**COMPLETE AND VERIFIED, first dispatch.** New `VerificationReportExporter` writes
`KOPRUQ_SOLVER_Verification_Report.xlsx` (Summary/Displacements/Reactions/Beam Forces/Elastic
Links/Diagnostics) via `xlnt`'s write API, reusing the exact same `ComparisonRow`s and
diagnostics GUI-5 already computed and showed on screen -- no separate recomputation; the
Summary sheet's category totals call the same `summarizeCategory()` the Özet tab uses (moved to
`ResultMatcher.h` so both call sites share one implementation). Metadata includes source
filenames, solver version (from `results.json` itself), tolerances used, and execution
timestamp. `MainWindow` now caches the last comparison's rows as members so `onExport()` reuses
them verbatim. Disclosed simplification: worksheet header labels are ASCII-only (data itself is
written verbatim) to avoid an unverifiable `xlnt` write-encoding risk with no local build to
check it against.

Verification: `release.yml` run `39548523252`, all five Windows presets `success`. **242/242
tests** (5 new `VerificationReportExporter` tests, all passing on the first attempt, including
row/column layout checks against the writer's real output). GUI smoke test confirmed another
clean 5s launch/close. No existing test modified.

## GUI-7 — cross-platform build/release validation — COMPLETE (2026-10-22)

**Verified.** `release.yml` run [`39760415174`](https://github.com/zharput/KOPRUQ-SOLVER/actions/runs/39760415174)
(commit `7bf04cb`): all six jobs green (`ninja-debug/release/asan/analyze/gui-release` on Windows,
`gcc-gui-release` on Ubuntu). **242/242 tests on both platforms.** The real GUI executable
launched (under `xvfb-run` on Linux), stayed alive 5s, closed cleanly, on both platforms. The
Linux GUI vcpkg binary cache (427MB) saved successfully for the first time this arc.

This closed a 13-fix debugging arc (gperf/libltdl/X11-XCB `-dev` packages, disk space, a `qtbase`
manifest-feature trim that had to be restored piece by piece with evidence from Qt's own source,
and finally a `Q_IMPORT_PLUGIN(QXcbIntegrationPlugin)` fix for vcpkg's static-by-default Linux
triplet -- the arc's only application-source-code change). Full narrative:
`docs/SOLVER_ROADMAP.md`'s "GUI-7" section. Separately, the user's approved CI caching
optimization (`restore-keys` on all four `vcpkg ... cache` steps in `release.yml`) was applied in
the same arc.

**Available for next steps:** the user has already re-exported the four MIDAS Excel reference
files as `.xlsx` (`xmidas/Beam Force.xlsx`, `Displacements(Global).xlsx`, `Elastic Link.xlsx`,
`Reaction(Global).xlsx`, untracked alongside the old `.xls` files) -- these are ready for the
first real end-to-end manual verification of the whole GUI-2..GUI-6 pipeline against actual MIDAS
data, which CI alone cannot drive (needs an interactive file-selection/compare/export session).
Not yet attempted.

GUI-0 through GUI-7 are now all complete and verified. This was the QT6 VERIFICATION GUI task's
final milestone.

## Windows portable GUI package — COMPLETE (2026-10-22, post-GUI-7)
The published `kopruq-verification-gui-windows-x64` artifact was initially just the bare 300KB
exe + `resources/dark.qss` (the `windeployqt` output `build/gui-release/` actually contained was
never uploaded) and lacked `kopruq_solver_cli.exe` entirely. Fixed, user-approved,
`release.yml`-only: broadened the upload to the full deployed set and added explicit pre-upload
checks (GUI exe, CLI exe at `SolverRunner::locateCli()`'s exact first candidate path, core Qt6
DLLs, `platforms/qwindows.dll` all required or the job fails naming what's missing) plus a
standalone `kopruq_solver_cli.exe --version` invocation on the `windows-2022` runner (no system
Qt6 install -- already the target "clean machine").

**Verified**, `release.yml` run [`39780356141`](https://github.com/zharput/KOPRUQ-SOLVER/actions/runs/39780356141)
(commit `99055ac`): all six jobs green, **242/242 tests**. Downloaded and inspected the zip
directly: **23 files, 20.3MB** -- both exes (same directory), all Qt6 DLLs, every plugin
subdirectory `windeployqt` populated, `xlnt.dll` and other vcpkg-applocal deps, `resources/`; no
debug/object/source/test files. All eight user-specified acceptance criteria passed. Full detail:
`docs/SOLVER_ROADMAP.md`'s "Windows portable package fix" entry.

**Download**: GitHub → repo → Actions → the `release.yml` run above → Artifacts →
`kopruq-verification-gui-windows-x64` → extract the zip anywhere and run
`kopruq_verification_gui.exe` directly; no installer, no separate Qt6 install needed.

## Per-category "Karşılaştır" checkbox (2026-11-16, post-GUI-7)
User-requested GUI-5 refinement: each of the four Excel reference rows in `FileInputPanel` now
has its own "Karşılaştır" checkbox (not the MIDAS model row -- it isn't a comparison category).
A checkbox is enabled only when its row's file is actually usable (right extension, exists on
disk); it defaults to checked the moment a file newly becomes usable, and is force-unchecked
whenever the file stops being usable -- but if the user manually unchecks an already-usable row,
that choice is preserved across redundant `revalidate()` calls (e.g. re-selecting the same path).
`MainWindow::onCompare()` now skips a category when its file is selected but unchecked, logging a
distinct diagnostic ("... dosya seçili ama \"Karşılaştır\" işaretli değil...") rather than the
"not selected" one. This lets the user keep all four paths filled in across runs but choose, run
by run, exactly which categories actually get compared (e.g. only Beam Force) without re-browsing
for files each time. `verification/` (the comparison engine itself) is unchanged -- this is a
GUI-only filter on which categories get invoked at all; Elastic Link was already optional (no
file required) and remains so, now with the same explicit per-run control as the other three.
Built and smoke-tested locally (`scripts/configure.ps1 Gui` / `build.ps1 Gui` / `run-gui.ps1`);
local `ctest` showed 4 pre-existing, unrelated failures (`Cli.MidasNxJsonFormatOption`,
`MidasJsonImporter.*`) because the user's local `xmidas/deneme1.json` had been deleted from the
working tree since GUI-7 (at the time, still present in git HEAD, so CI was unaffected) -- not
caused by this change and not touched here.

**UPDATE (2026-11-17): `xmidas/deneme1.*` (including `deneme1.json`) permanently removed from git
itself, by explicit user instruction -- no longer just a locally-deleted, git-recoverable file.**
The user confirmed they deleted this sample from their own machine and asked for it to be removed
from git history going forward too (commit `e688f53`); the old `xmidas/deneme1.json`
naming/fixture scheme is retired in favor of the current `verification/models/01`-`17` set. This
is now a **permanent, irrecoverable** (from this point in history forward) loss of that one real
sample, not a transient local-environment issue -- CI *is* now affected, not just a developer's
local machine, since there is nothing left in the repository to restore. The following 5 tests
depend on this file and will **always fail from now on**, by explicit user decision, left
untouched rather than silently rewritten or deleted (CLAUDE.md: preserve existing tests, never
hide a failure by weakening/removing a test) -- this is a known, accepted, permanent limitation,
not a bug to chase or a transient CI blip to retry:
- `Cli.MidasNxJsonFormatOption`
- `MidasJsonImporter.ImportCountsMatchIndependentlyComputedTotals`
- `MidasJsonImporter.ReportsElasticLinkRejectionCountAndNodeBoundingBox`
- `MidasJsonImporter.ImportedModelPassesValidation`
- `MidasJsonImporter.StaticSolveOfImportedModelDoesNotThrowAndReportsAStatus`

Every CTest run reported elsewhere in this document and in `docs/SOLVER_ROADMAP.md` from
2026-11-16 onward that cites "5 pre-existing, unrelated `xmidas/deneme1.json`-missing failures" is
describing exactly these same 5 tests -- the count has been stable and unrelated to every other
change made in that period, consistent with this being a single, isolated, now-permanent fixture
loss rather than a drifting set of regressions.

**UPDATE (2026-11-19): these 5 tests removed entirely, by explicit user instruction** ("xmidas\
deneme1.json testini tamamen iptal et, bununla ilgili raporlarda FAIL diye sonuç verme" --
cancel the deneme1.json test entirely, stop reporting FAIL for it). Rather than continuing to
carry 5 permanently-failing tests in every report, the four `MidasJsonImporter.*` tests that
depended on `loadSample()`/`xmidas/deneme1.json` were deleted outright from
`tests/unit/midas_json_importer_tests.cpp` (their file-dependent fixture helper removed with
them; the file's other tests, which use self-contained synthetic JSON, are untouched), and
`Cli.MidasNxJsonFormatOption`'s `xmidas/deneme1.json` sub-case was removed from
`tests/unit/io_tests.cpp`, keeping that test's other (still-valid, fixture-independent)
assertions about `--format` usage errors and invalid-JSON rejection. `CMakeLists.txt`'s now-unused
`KOPRUQ_SOURCE_DIR` compile definition on the importer-tests target was removed too.
**Verified: Debug CTest 258/258 PASS, gui-release 288/288 PASS -- 100% passing, no permanent
failures left.** Every CTest/report baseline from this point forward should read "all tests
passing," not "N/M, same 5 pre-existing failures" -- that baseline no longer applies. If
`xmidas/deneme1.json`-equivalent real MIDAS data is ever supplied again, new tests can be written
against it from scratch (the current `verification/models/01`-`19` set already covers this need
going forward).

## GUI V2.2 — 3D Model Viewer, Stages A/B/C — COMPLETE (2026-11-17)
Added a read-only OpenGL "Model Görünümü" tab beside the existing "4. Sonuç Karşılaştırma
Tabloları" tab (`apps/verification-gui/ModelViewerWidget.{h,cpp}`, `QOpenGLWidget` + desktop
fixed-function pipeline). Model geometry comes exclusively from the existing
`kopruq::io::detail::readModelFile(..., "midas-nx-json", ...)` / `MidasJsonImporter` path (the
same one the CLI/solver use) and is gated by the same `kopruq::MatrixAssembler::assemble()`
validation used before a solve -- a failed import/validation always clears the viewer rather
than leaving a stale model displayed. No new JSON parser was written.
- **Stage A (Viewer Foundation)**: tab, camera (rotate/pan/zoom/fit/reset/isometric/top/front/
  side), node/beam wireframe drawing. Commit `c2e6b92`.
- **Stage B (Model Interaction)**: optional Node ID/Element ID labels (default OFF, performance),
  click-to-select node/beam (screen-space picking via the render MVP matrix), an Element/Node
  Information side panel built only from real model fields (coordinates, restraints, section/
  material ids, length). Commit `16ff859`.
- **Stage C (Structural Display)**: support symbols from the actual restraint pattern only
  (Fixed/Pinned/Roller canonical patterns; any other pattern draws a distinct generic marker,
  never a guess), elastic links (`kopruq::Model::springs`) as separate `#A78BFA` lines, local
  axis arrows computed via `kopruq::Beam3D::axes()` directly (never a separate visual-only
  calculation), a corner XYZ camera-following gizmo, and toolbar toggles (Supports/Elastic Links
  default ON, Local Axes default OFF). Commit `70d4acf`.
- `vcpkg.json` gained qtbase's `"opengl"` feature; `CMakeLists.txt` links `Qt6::OpenGL`/
  `Qt6::OpenGLWidgets` (+ `opengl32` on Windows).
- Verified after each stage: `ninja-gui-release` build succeeds; CTest **269/274 PASS** at every
  stage, with the exact same 5 pre-existing/permanent `xmidas/deneme1.json`-removal failures
  listed above -- no regressions introduced by the viewer work at any stage.
- Not yet done: interactive mouse-driven GUI testing (standing constraint from this project's
  mouse-automation privacy incident -- CLI/code-level verification only).
- Deferred by the approved spec to a later version: deformed shape, force/stress diagrams, solid
  section rendering, traffic/moving load visualization, BIM export, CAD editing.

## GUI V2.2 — post-Stage-C corrections (2026-11-17/18)
User-driven follow-up fixes after the Stage A/B/C viewer was approved, all verified against the
same stable 269/274 `gui-release` CTest baseline (the 5 pre-existing failures listed above; no
other regressions at any step) before being reported and approved:
- **Camera/axis corrections** (`ModelViewerWidget.cpp`), iterated through several rounds of user
  screenshot feedback until the orientation was rigorously re-derived (not guessed) from the
  screen-projection formulas `R = cross(forward, up)`, `U = cross(R, forward)`: Z-up convention
  (`ba33a85`), X/Y swap fix (`c3947b6`), +X sign fix (`5daeee6`), full re-derivation to match the
  user's reference axis diagram (`15fa0e7`), and Top/Front/Side view-preset screen-direction
  fix, derived the same rigorous way from the start and confirmed correct on the first attempt
  (`4fe236c`). Final camera formula: `dir = (-cos(pitch)*cos(yaw), cos(pitch)*sin(yaw),
  sin(pitch))`, `upVector_ = (0,0,1)` except the Top preset which uses `(0,1,0)`.
- **GUI layout/icon reorganization** (`ba33a85`): icon-based toolbar (`QStyle::standardIcon`), a
  right-hand "Görünüm Seçenekleri" checkbox panel, single-line selection info bar -- scope
  explicitly limited to layout/icons only, per the user's own choice, no new rendering features.
- **Single-line element/node info** (`87f57f0`): `describeNode`/`describeBeam` rewritten to one
  `QStringLiteral(...).arg(...)` line each instead of a multi-line block, per a reference
  screenshot the user supplied.
- **Results Comparison table legibility** (unrelated to the 3D viewer; `791e1fa`, `9f2d60a`,
  `fa1ff69`): PASS/FAIL row coloring (green/red) added to every comparison table and the category
  summary table; N/A cells changed from an empty string to a visible "—" placeholder.
  **Root-cause bug found and fixed in `fa1ff69`**: every comparison table had
  `setSortingEnabled(true)` set once at construction (`ResultsComparisonPanel::buildTable`), and
  `QTableWidget` re-sorts its rows after *every* `setItem()` call when sorting is active --
  filling a row column-by-column let Qt move that row mid-fill, scattering its own cells across
  other rows. Only one row per table ended up fully populated; this matched the user's
  screenshots exactly (SOLVER Değeri/MIDAS Değeri/Durum blank even on rows whose Yük Durumu
  showed a value). Fixed by disabling sorting before populating each table (detail tables,
  diagnostics table, category table) and re-enabling it after. Also added N/A/Unsupported row
  coloring (orange) alongside PASS/FAIL, extended to the category table's own PASS/FAIL/N/A
  status derivation (previously collapsed an all-N/A category to "PASS") and to the Özet tab's
  N/A count tile.
- Verified 2026-11-18: `gui-release` CTest **269/274 PASS**; `debug` (non-GUI target set) CTest
  **239/244 PASS** -- both runs show only the same 5 pre-existing/permanent
  `xmidas/deneme1.json`-removal failures listed above, no regressions.

## GUI V2.2 Stage D — load visualization (2026-11-18, commit `1254ca0`)
User requested the viewer display loads, local section orientation, and solve results (reactions/
displacements/forces); asked to use VTK "if needed." VTK was attempted first (see below) but is
currently blocked by a Windows environment limitation outside the project's control, so Stage D
proceeds on the existing Qt OpenGL (fixed-function) viewer, explicitly disclosed rather than
silently dropping the VTK decision -- to be revisited once the environment issue is resolved.

- Adds a "Yük Durumu" (load case) selector to the viewer toolbar and a "Yükler" display toggle
  (default OFF) to the options panel.
- Draws arrow glyphs for the selected `kopruq::io::ModelInput` load case's `NodalLoad` (force/
  moment resultants), `MemberPointLoad`, and `MemberLoad` (distributed, sampled at 5 stations
  across `[start,end]` with linear `w1`->`w2` interpolation) -- the exact structures the solver
  consumes, never a separate representation. Local-axis member-load directions are resolved via
  `kopruq::Beam3D::axes()` on the real member geometry, exactly as the existing local-axis arrows
  already do. Forces (#FBBF24) and moments (#EC4899, additionally ringed at the base) are colored
  separately; each arrow's length is scaled only relative to the largest magnitude of its own kind
  within the selected case, never as an absolute physical length.
- Self-weight (`LoadCase::self_weight`) is deliberately **not** drawn: its true per-member force
  depends on section area and material unit weight, which would mean either duplicating the
  solver's self-weight calculation in the viewer or showing a misleadingly arbitrary arrow length
  -- left as a documented limitation.
- `NodalLoad::values(0..5)` index layout (Fx,Fy,Fz,Mx,My,Mz) confirmed directly against
  `ModelJson.hpp`'s own writer code before use, not assumed.
- Verified: `gui-release` CTest **269/274 PASS**, same 5 pre-existing failures, no regressions.
  Interactive mouse testing not performed (standing constraint this session: CLI/code-level
  verification only, after an earlier screenshot-automation privacy incident).
- Not yet done (next planned): Stage E (Kesit Yönleri -- section/local-axis orientation display
  beyond the existing axis arrows), Stage F (Reaksiyon/Deplasman/Kuvvet -- requires wiring actual
  solve results into the viewer for the first time).

### VTK attempt — blocked (2026-11-18)
Added `vtk` (minimal `opengl`+`qt` features, default-features off) to `vcpkg.json`'s `gui`
feature and ran a vcpkg install. Failed partway through building `qtdeclarative` (one of VTK's
Qt-integration dependencies):
```
fatal error C1083: Cannot open compiler generated file: '': Invalid argument
```
preceded by vcpkg's own warning that qtdeclarative's buildsystem uses very long paths and may
fail. Root cause: the local vcpkg install's buildtree path
(`C:\_ZHarput_Data\Tools\vcpkg\buildtrees\qtdeclarative\...`) combined with Qt's own long
generated filenames exceeds Windows' classic `MAX_PATH` (260 characters) limit. This is a
machine/environment limitation (shared vcpkg install location, Windows long-path support), not a
project code issue. Reverted the `vcpkg.json` change (confirmed it was blocking the normal
build/run workflow, including the VS Code "Run GUI" task, since every reconfigure re-attempted
the same failing install) and rebuilt/retested to confirm **269/274 PASS** restored with no
VTK dependency. Leftover partial buildtree directories
(`C:\_ZHarput_Data\Tools\vcpkg\buildtrees\{qtdeclarative,vtk,vtk-compile-tools}`) were removed as
routine disk cleanup; they held no lock and were not referenced by any active build.
Resolving this (Windows long-path registry setting, moving/relocating the shared vcpkg install,
or `subst`) requires an IT-authority decision the user asked to defer -- VTK stays out of scope
until that decision is made.

## Phase 5.4 revision + 5.4b — ElasticLink generalized, Joint Spring added (2026-11-18)
User-requested, with an explicit formulation decision taken first (see Approval Request below --
approved): (1) generalize the existing two-node `SpringLink` ("ElasticLink") to non-coincident
nodes, matching MIDAS NX's own `ELNK` General-link behavior (no lever-arm coupling added, by the
user's explicit choice over the alternative of adding rigid-offset coupling); (2) add a new,
conceptually separate single-node elastic support ("Joint Spring", MIDAS NX `NSPR` Nodal Spring).
`RigidLink` (already two-joint, kinematic) needed no change.

- `include/kopruq/elements/SpringLink.hpp`: `checkSpringNodesCoincident()` removed, all 3 call
  sites updated (`MatrixAssembler.hpp`, `Validate.hpp`, `ModelJson.hpp`); header comment rewritten
  to document the new scope and the deliberately-kept lever-arm-coupling limitation.
- `include/kopruq/elements/JointSpring.hpp` (new): `JointSpringConnection`
  (id, node, local_x, reference_y, stiffness -- same per-DOF-diagonal convention as `SpringLink`),
  `jointSpringGlobalStiffness()`.
- `Model::joint_springs` (new field, own id space); `MatrixAssembler::assemble` adds each joint
  spring's 6x6 global stiffness directly to its node's own DOF block; `Validate.hpp` validates it
  the same way springs are validated (id/node/stiffness/orientation).
- `LinearStaticSolver.hpp`'s `recoverAssembled`: new `AnalysisResults::joint_springs` (per-spring
  force/energy); its force is folded into the model-level force/moment equilibrium check for any
  DOF component not separately restrained at that node, with the **negative** sign (the ground's
  reaction through the spring, Newton's third law) -- found via
  `JointSpring.TiltedLoadedSpringSatisfiesEquilibrium` initially failing with
  `force_equilibrium_error == 1.0` (fully unbalanced) when the sign was wrong; fixed and
  re-verified at `< 1e-9`.
- Native JSON I/O: `joint_springs` array (reader + writer, `ModelJson.hpp`), new
  `schemas/model.schema.json` `$defs/joint_spring` entry (`ModelJson.SchemaFileMatchesReaderKeys`
  updated to check it, not weakened).
- Tests: `spring_io_tests.cpp`'s `RejectsNonCoincidentNodes` replaced with
  `AcceptsNonCoincidentNodes` (hand-derived closed form, since the old assertion tested behavior
  the user explicitly asked to change -- not a hidden failure). New files
  `tests/unit/joint_spring_tests.cpp` (3 tests: axis-aligned stiffness vs. dense formula, simple
  axial closed form, tilted/general-load equilibrium) and `tests/unit/joint_spring_io_tests.cpp`
  (8 tests: parse/solve parity, defaults, rejections, independent id space from `springs`).
- **Not done this session**: MIDAS NX `ELNK`/`NSPR` import wiring. Model `07 Basit Kiris Uniform
  yayli`'s real `ELNK` record (node 10 at Z=-0.5 to node 1 at Z=0.0, 0.5 m apart, `SDR` stiffness,
  `ANGLE`/`DR` fields of genuinely unconfirmed meaning -- no MIDAS documentation available to this
  project) would be the natural verification target for `ELNK` import, but `ANGLE`/`DR`'s exact
  meaning needs to be resolved empirically (try an interpretation, solve, compare against the
  model's own real `Beam Force.xlsx`/`Displacements(Global).xlsx` MIDAS reference) before trusting
  an import mapping -- not attempted yet, to avoid fabricating an unverified interpretation. `NSPR`
  import has no real sample at all (0 records in all 17 MIDAS sample exports), so it cannot be
  verified yet either.
- Verified: Debug CTest **250/255 PASS** (gui-release: **280/285 PASS**), same 5
  pre-existing/permanent `xmidas/deneme1.json`-removal failures as every prior baseline in this
  document, no regressions; 11 new tests all passing.

## Phase 5.4 addendum — MIDAS ELNK import wired and empirically verified (2026-11-18)
Followed up the same day: `MidasJsonImporter.hpp` now imports `ELNK` (`LINK:"GEN"`) records as
`SpringConnection` (previously always rejected). Orientation reuses the already-confirmed
`ELEM`/`BEAM` node1->node2 + `ANGLE` convention (`midasReferenceY`); `SDR` maps to local
`kx..krz`; `R_S` must be all-false (any release flag true is rejected, not guessed); `DR` is read
but not used (no formulation concept to apply it to).

**Verification**: a standalone diagnostic (not committed) imported, solved and compared
`verification/models/07 Basit Kiris Uniform yayli`'s real MIDAS export -- one non-coincident ELNK
(node 10 at Z=-0.5 linked to node 1 at Z=0.0) -- against that model's own real MIDAS
Displacements/Beam Force/Reaction `.xlsx` files: **168/168 rows PASS, 0 FAIL, 0 N/A**. This is
strong empirical evidence the orientation/stiffness interpretation (and the choice to ignore
`DR`/require `R_S` all-false) is correct for MIDAS's `GEN` link type. A permanent regression test,
`tests/unit/midas_elastic_link_tests.cpp`, embeds model 07's real DBG payload inline (so it runs
in CI without depending on the untracked `verification/models/` directory) and checks node 1's
displacement/rotation plus node 9/10's reactions against values transcribed from that same real
export; node 10 is fully restrained and touches the structure *only* through the ELNK spring, so
this is a genuine check of the import's correctness, not a smoke test.

Verified: Debug CTest **251/256 PASS**, gui-release **281/286 PASS**, same 5 permanent failures,
no regressions.

Not yet done: MIDAS NX `NSPR` (Joint Spring) import -- still blocked on the complete absence of
real `NSPR` sample data (0 records in all 17 MIDAS exports available to this project).

## Phase 5.4b / 5.1 addenda — MIDAS NSPR + FRLS import; SPLC ANGLE generalized; new T-section
## blocker found (2026-11-18, user-supplied models 17/18/19)
User supplied 3 new real MIDAS models and asked for tests. **NSPR now unblocked** (model 19
supplies real sample data the previous entry above said was missing) -- imported as
`JointSpringConnection` (`TYPE:"LINEAR"` only; `F_S`/`DAMPING`/`Cr`/`EFFAREA`/`DK` must be at
default, rejected otherwise). **Found and fixed a real bug during verification**:
`NodeResults::reaction` did not include a joint spring's force for a node's unrestrained
components, so every joint-spring-only node's reaction reported as zero -- MIDAS's own export
reports it as a reaction, confirmed by the mismatch (0/12 reaction rows matching before the fix,
84/84 after). Verified against model 19's real data: Beam Force 552/552, Reaction 84/84,
Displacements 293/294 (one small isolated rotational mismatch, not investigated, consistent with
other already-accepted small discrepancies elsewhere). New tests:
`tests/unit/midas_joint_spring_tests.cpp` (5).

**FRLS (beam end release / "mafsal") import also added** (`Beam3DConnection::releases`, same
per-DOF-flag convention as `CONS`'s `CONSTRAINT`); parsing verified correct against model 18's 10
real `FRLS` records, but full numerical verification is blocked by a new gap below.

**SPLC `ANGLE` generalized**: previously any nonzero `ANGLE` was rejected outright; now any
multiple of 90 degrees within the plane `DIR` names resolves to one of `DIR`'s two axes (0=first
letter, 90=second, mod 180). Needed because model 17's `SPECY` case has `DIR="XY"` `ANGLE=90` --
the same `DIR` string as model 16's already-verified `ANGLE=0` case, so `DIR` alone could not
have been the whole story. Confirmed physically sensible against model 17's real
`Reaction(Global).xlsx` (`SPECX(RS)` rows dominated by FX, `SPECY(RS)` by FY).

**New blocker, not resolved**: MIDAS `SECT.SHAPE:"T"` (T-section) is unsupported
(`MidasSectionLibrary.hpp` only has SB/B/SR) -- blocks full solve/verification of both model 17
and model 18 (both use it). Implementing it needs an independently-derived T-section
area/Iy/Iz/J/shear-area formula set (a real Phase 7S task), not attempted this session; priority
is the user's decision.

Verified: Debug CTest **256/261 PASS**, gui-release **286/291 PASS**, same 5 permanent failures,
no regressions; 16 new tests all passing.

## T-section added; models 17/18 solve but show a new, unresolved discrepancy (2026-11-18)
`MidasSectionLibrary.hpp` gained `tSection()` for MIDAS `SHAPE:"T"` (composite flange+web
parallel-axis decomposition for area/IH, no-offset IB, classical open-section J approximation,
web-only/flange-only shear areas) -- unblocks models 17 and 18's "BASLIK" pier-cap section. New
hand-computed test `MidasSectionLibrary.TSectionMatchesHandComputation_Baslik`.

Both models now solve without error. Verified against model 18's real MIDAS export via a
standalone (uncommitted) diagnostic:
- **Reaction: 96/96 PASS** (exact match -- the overall section/stiffness is sound enough for
  global equilibrium).
- **Beam Force: 1806/1872 PASS, 66 FAIL.** Most failures are a **consistent exact sign flip** in
  Moment-z for one specific member group (elements 62-71, section 4/SR, an arch or pier leg) --
  not a magnitude error, a sign convention mismatch, and NOT related to the T-section (these
  elements use SR, not T). A smaller set of elements (17-24) show a constant ~10% Axial force
  offset, and a few more (57-61, 72-75) show smaller (~1-3%) Moment-y/Shear-z differences.
- **Displacements: 919/948 PASS, 29 FAIL**, concentrated at nodes adjacent to the same member
  group (DZ off by up to ~5%, RY by a few %).

**This is a genuine, unresolved finding, not accepted or worked around.** It was not present as a
blocker before (the T-section gap masked the whole model from solving at all) and is unrelated to
FRLS releases (none of the failing elements carry a release) or the T-section formula itself
(the failing elements use SR). Candidate causes not yet investigated: an `STYPE` field on these
`ELEM` records (currently ignored, might indicate a special member type MIDAS treats differently,
e.g. a cable/truss-like element within an otherwise-BEAM-typed arch); a local-axis orientation
effect specific to this member group's geometry. Left for the user's decision on priority --
**model 17 (also now solving thanks to the T-section fix) has not yet been numerically verified
at all** (only confirmed to solve without error); its own comparison is pending.

Verified: Debug CTest **257/262 PASS**, gui-release **287/292 PASS**, same 5 permanent failures,
no regressions.

## Moment-z sign-convention bug found and fixed via model 18 (2026-11-19)
Root cause of the Moment-z sign-flip finding above: `verification/ResultMatcher.cpp`'s
`kBeamForceFlipsAtIEnd` table assumed Moment-z behaves like Moment-y for the I/J sign-flip rule,
by a symmetry argument its own comment disclosed as NOT yet empirically confirmed (every model
available before model 18 is planar, so Moment-z was always structurally zero and this part of
the rule was never actually exercised). Model 18's vertical pylons (real, nonzero out-of-plane
bending) falsify the assumption outright -- every Moment-z row was an exact sign flip. Fixed by
moving Moment-z into the Axial/Shear/Torsion group instead (flips at I, not J). This also has a
mathematical explanation, not just a data fit: the xy and xz bending planes are not symmetric
under this project's right-handed local-axis convention (z = x cross y directly, y = z cross x,
an extra cyclic step), which is exactly why Moment-z's flip is the opposite of Moment-y's.

Verified: model 18's Beam Force comparison improved from 1806/1872 to **1834/1872 PASS** (every
Moment-z sign-flip failure resolved). Full regression: Debug CTest **258/258**, gui-release
**288/288** -- 100%, no regressions (every other model is planar for Moment-z, so structurally
insensitive to this fix).

**Remaining 38 Beam Force failures + 29 Displacement failures, now better understood**: all are
in the "POINT" load case (a separate `CNLD` horizontal point-load case, not the "FORCE" uniform-
load case) for beam forces, and in "FORCE" for displacements; both are small (~2-10%,
magnitude-only, no more sign flips) and localized to elements using the newer `T`/`B`/`SR`
sections (never the plain `SB` deck sections). Reactions still match **exactly** (96/96) for
both load cases, meaning global equilibrium is correct but internal force/displacement
*distribution* differs slightly -- the signature of a stiffness-ratio sensitivity in this
statically-indeterminate 3D structure (`USE_SHEAR_DEFORM=true` here, unlike most prior models).
Likely explanation: the already-disclosed approximate shear-area formulas for `T`/`B` (and to a
lesser extent `SR`, though that one is classically exact) are precise enough for force-only
checks but not precise enough to reproduce MIDAS's exact internal redistribution in a flexible,
indeterminate structure. Not investigated further this session -- improving shear-area precision
for these shapes would need either MIDAS's own effective-shear-area formula (not accessible) or a
more sophisticated thin-walled-section shear-flow derivation, a real engineering task requiring
its own scoping.

**Model 17 now numerically checked (2026-11-19)**: its `NODE`/`ELEM`/`SECT`/`MATL`/`CONS`/`BMLD`
are byte-identical to model 18's (confirmed by direct JSON comparison) -- the two are the same
bridge, model 18 adding `FRLS` hinges ("mafsal") and a `CNLD` point-load case, model 17 adding
modal/spectrum requests instead. Model 17's static "FORCE" case: elements 1-2 (nearest the
roller support) match exactly; from element 3 onward, **beam force shows 373/936 FAIL**
(reactions and the first elements still correct, errors modest -- a few % to ~10% -- and growing
away from the support, no sign flips). This is the SAME underlying issue family as model 18's
remaining discrepancy (likely shear-area approximation precision for `T`/`B`/`SR`), but far more
visible here: model 18's `FRLS` hinges release some of the structure's redundancy (closer to
statically determinate, so less sensitive to a small stiffness error); model 17, fully continuous
with no hinges, is the MORE indeterminate case and amplifies the same small imprecision across
many more elements. Spectrum-case (`SPECX(RS)`/`SPECY(RS)`) rows were not checked this session
(would need the same modal+spectrum verification approach already used for model 16).
Not investigated further -- same root cause and same remediation path as model 18's open item
above.

## Rigorous shear-area formulas confirm the hypothesis: model 18 reaches 100% (2026-11-19)
User instruction: prioritize replacing the approximate T/box shear-area formulas with classical
theory (MIDAS's own effective shear area data is inaccessible). Added
`detail::timoshenkoShearArea()` (`MidasSectionLibrary.hpp`): the standard Zhuravskii shear-stress
distribution gives a closed-form energy-consistent shear area As = I^2 / integral(Q(y)^2/b(y) dy)
for any section built from rectangular strips -- replaces the earlier coarser "web/flange alone"
(T) and "parallel walls only" (box) approximations for both AsH and AsB. An initial draft
accumulated Q(y) via many small discrete increments and had an O(1/N) bias; caught during the
function's own self-test (`TimoshenkoShearAreaMatchesClosedFormRectangle`, which must reproduce
the EXACT k=5/6 rectangle result) and fixed by evaluating Q(y) via its exact closed-form
antiderivative within each constant-width strip instead, leaving only the final (smooth quartic)
integration numerical.

**Verified against real MIDAS data -- confirms the shear-area approximation was the actual root
cause**: model 18's Beam Force/Reaction/Displacement comparison, previously 1834/1872, 96/96,
919/948, is now **1872/1872, 96/96, 948/948 -- 100% PASS across the board.** Model 17 (same
geometry, no `FRLS` hinges, more statically indeterminate so more sensitive to every approximate
property) improves substantially too: FORCE-case beam force from 563/936 to 817/936 PASS,
reactions from 17 to 5 mismatches, displacements from 441/474 to **474/474 PASS**. Model 17's
remaining residual is not investigated further this session -- likely a different, smaller,
already-disclosed approximation (e.g. the open-section/Bredt torsion constant J), newly visible
now that shear area is no longer the dominant error source.

Verified: Debug CTest **259/259 PASS**, gui-release **289/289 PASS** -- both 100%, no
regressions.

## Torsion constant investigated; model 17 tested and verified (2026-11-19)
User instruction: fix the torsion-related parts too (prioritizing J after shear area), then test
and verify model 17. Added `detail::torsionConstantFEM()` (`MidasSectionLibrary.hpp`): solves St.
Venant's torsion via the membrane analogy (Laplacian(phi)=-2, phi=0 on the boundary,
J=2*integral(phi dA)) -- the exact formulation for an arbitrary section, numerically solved via a
finite-difference Laplacian (Eigen sparse Cholesky). Verified correct against two independent
references (tabulated square coefficient, this file's own closed-form rectangle), both within
~0.03-0.1%.

**Tried wiring it into the T-section's J (replacing the thin-strip-sum approximation) -- made
things WORSE, not better.** Model 17's real MIDAS-data match regressed (Beam Force PASS 817/936
-> 757/936, reaction mismatches 5 -> 11) while model 18 (same section) was unaffected either way.
This is strong empirical evidence MIDAS's own DBUSER general-section torsion constant uses the
same simple thin-strip-sum convention internally, not an exact PDE solve -- so matching MIDAS's
convention, not the more "physically rigorous" answer, is what actually helps here. **Reverted**
`tSection()`'s J to the strip sum; `torsionConstantFEM()` itself is kept (correct, tested, useful
for a future shape it may genuinely suit) but not wired into any shape's J this session. This is
an important, disclosed finding, not a quiet revert: "more rigorous" is not automatically "more
correct" when the goal is reproducing a SPECIFIC other program's own (possibly simplified)
convention, not the true physical answer.

**Model 17 tested and verified** (after the revert, i.e. with the shear-area fix but the original
torsion formula): its real MIDAS reference data gives **FORCE-case Beam Force 817/936 PASS**,
**Reaction 43/48 PASS** (5 small residual mismatches, 1-2%), **Displacement 474/474 PASS**. The
small remaining FORCE-case residual is not pursued further, per the user's explicit instruction
to stop at this checkpoint.

Verified: Debug CTest **261/261 PASS**, gui-release **291/291 PASS** -- both 100%, no
regressions.

**Modal + spectrum results also verified** (user approved continuing to this check): `ModalSolver`
(lumped mass) against model 17's real `Eigenvalue Mode.xlsx` gives **76/80 PASS** (40 modes'
frequency+period, 2 modes off by ~1.1-1.3%, same pattern of isolated small mode divergence as
models 13-15's already-accepted residual). `ResponseSpectrumSolver` (CQC, lumped mass) combined
reactions for both `SPECX`/`SPECY` cases, checked by hand against model 17's real
`Reaction(Global).xlsx` `SPECX(RS)`/`SPECY(RS)` rows at nodes 54 and 58 (8 values): **all within
0.03-1.1% of MIDAS's own reference** (most under 0.1%), confirming the SPLC `ANGLE`-based
direction resolution, CQC combination and lumped-mass pipeline are all correct for this model too,
matching the quality of model 16's prior verification.

## Next autonomous action
Phase 3.2, Phase 4, Phase 5 (5.1-5.6), Phase 6 (6.1, 6.1b, 6.2, 6.2b; 6.3 cancelled), Phase 7A,
7B (including ETMP thermal wiring), 7C, 7D, 7M, 7S(1) and 7T are all complete and verified -- CI
run `39265009361`, commit `a92f045`, all three matrix jobs (gcc-debug, gcc-asan, gcc-release)
`success`. GUI-0 through GUI-7 (Qt 6 Verification GUI, the entire QT6 VERIFICATION GUI task) are
now all complete and verified (see above). Per the task's own "stop for approval after each
milestone" instruction, development is paused at this final milestone. Natural next step, if
desired: a real end-to-end manual verification session against the user's own re-exported
`.xlsx` files (see above) -- not yet performed, since it needs interactive use of the GUI itself.
Other candidate next steps (unrelated to the GUI, say which, or propose a different scope):
1. **Phase 7 (production solver, unrelated to MIDAS import)**: sparse direct/iterative strategy,
   optional SuiteSparse, profiling/parallelism -- still not started.
2. Extend rigid-link support to ModalSolver/ResponseSpectrumSolver, or cut a `v0.1.0` release tag.
3. Confirm MIDAS's GTMP T1/T2 sign convention (e.g. from the user, who may have access to the
   official API manual) to unblock gradient-temperature import, if ever needed.

## Phase 7G complete — LCOM-GEN load-combination import, with nested CB expansion (2026-11-19)

`MidasJsonImporter.hpp` now imports `LCOM-GEN` records into `LoadCombination`, including a
memoized, cycle-safe recursive expansion of nested `ANAL=="CB"` references (combination-of-
combinations) into flat primitive-load-case terms — mathematically exact under linear statics,
so `LinearStaticSolver` itself is unchanged; only `iTYPE==0` (linear) and `ACTIVE=="ACTIVE"`
entries are accepted, others rejected with a clear diagnostic.

**Verified:** model 06 (`06 Cerceve-2`, 2 non-nested combinations) — 100% PASS, 1080/1080 beam
force, 60/60 reaction, 570/570 displacement, including the previously-`N/A` `COMBO-*` rows.
Full regression: 291/291 Release, 261/261 Debug (commit `d01f3f3`).

**Not verified this session (disclosed, not silently skipped):** model 10 (1 combination) and
model 21 (4 combinations, including the nested `COMBO-3`/`COMBO-4` case that is the more
rigorous test of the recursive-expansion path) both fail to solve for reasons unrelated to
combination import — unsupported `DBUSER` section shapes (`'H'` on model 10; `STRK`/`SOCT` on
16/188 elements of model 21). See Phase 7G.1 in the roadmap.

## Phase 7G.1 — new blockers found this session, not yet addressed (2026-11-19)

Investigating the user's "test models 19, 20, 21" instruction surfaced three distinct,
previously-undocumented gaps, none implemented yet (deliberately, to avoid fabricating unverified
section geometry — see roadmap Phase 7G.1 for full detail):
- **Model 19 (spring):** no new gap — already fully verified earlier this session (552/552 beam
  force, 84/84 reaction, 293/294 displacement). Nothing further needed.
- **Model 20 (truss, `20 Kafes`):** needs (a) `MATL.TYPE:"STEEL"` material resolution (only
  `resolveConcreteGradeMaterial` exists today), and (b) catalog/database section lookup by
  `SECT_I.DB_NAME`/`SECT_NAME` (e.g. `"UPN100"`), which `resolveDbUserSection` does not support
  (it only handles raw `vSIZE`-based shapes). Also notable: MIDAS models this truss with ordinary
  `TYPE:"BEAM"` elements (no `FRLS` releases), not an explicit pin-jointed truss type.
- **Model 21 (combo, `21 Combo`):** needs two new `SECT.SHAPE` resolvers, `STRK` and `SOCT`,
  whose `vSIZE`→geometry mapping is not confirmed from the data alone (2 and 4 meaningful `vSIZE`
  values respectively) — these block the entire model's solve (16/188 elements), including the
  nested-combination verification row still pending from Phase 7G.

None of these three gaps has been implemented. Resolving any one requires either authoritative
MIDAS section/material documentation or explicit user confirmation of the parametrization, per
the project's standing "never fabricate structural properties" rule.

## Model 20 (truss) mostly unblocked — STEEL material, TRUSS element, catalog sections, CONLOAD (2026-11-19, commit `b0f8d3a`)

Resolved 3 of the 4 Phase 7G.1 gaps above for model 20 specifically:
- `resolveSteelGradeMaterial` (`MidasMaterialLibrary.hpp`): EN1993-1-1 S3.2.6 fixed E=210000 MPa,
  Poisson=0.3, unit weight=78.5 kN/m^3, thermal expansion=1.2e-5/K for any `EN..(S)` steel grade
  (grade only changes fy/fu, unused by this linear-elastic solver). STEEL's design code/grade
  live on `MATL.PARAM[0]` directly, not a separate MATD record like CONC — confirmed empirically
  (model 20's own `MATD` is present in the export but empty).
- `MidasJsonImporter`'s `ELEM` loop now accepts `TYPE=="TRUSS"` (all 25 of model 20's elements,
  not `TYPE:"BEAM"` as an earlier investigation pass of this session mis-stated), building a
  `Beam3DConnection` with `Truss3D.hpp`'s existing, already-verified pin-ended release pattern —
  no new element formulation, reuses Phase 5.2.
- `resolveCatalogSection`/`iSection`/`channelSection` (`MidasSectionLibrary.hpp`): resolves a
  MIDAS catalog/database section reference (`SECT_I.DB_NAME`+`SECT_NAME`, not a raw `vSIZE`
  array) for `HEB160` (EN 10034) and `UPN100` (DIN 1026-1), idealized rectangular-flange
  geometry (no root fillets, same precision level as this file's other shapes).
- `BMLD`'s `CONLOAD` (concentrated member point load) now maps onto the solver's pre-existing,
  already-verified `MemberPointLoad`/`pointLoadEffect` pipeline — not a new solver capability,
  just a previously-missing importer mapping (only `UNILOAD`/distributed was wired before).

**Deliberately NOT implemented:** model 20's 3rd section (`L50x5`, shape `'L'`, 13 of 25
elements) is an unsymmetric equal-leg angle whose principal bending axes are rotated relative to
H/B — correctly representing it needs a product-of-inertia (Iyz) term `kopruq::Section` does not
have. This is a `Section`/`Beam3D` formulation change, not an importer mapping, and is out of
this checkpoint's scope — rejected with a clear diagnostic rather than approximated.

**Verified:** model 20 now imports and solves its 12/25 HEB160/UPN100 elements, both load cases
(`FORCE`, `POINT`) and their `COM1` combination. Full regression: 291/291 Release, 261/261 Debug,
unchanged.

## L50x5 angle (Iyz) resolved via import-time principal-axis rotation; a SEPARATE, deeper model-20 blocker found (2026-11-19, commit `9745c16`)

Per user approval ("Task 1"), implemented the equal-leg-angle Iyz gap from the previous entry --
`equalAngleSection` (`MidasSectionLibrary.hpp`) diagonalizes the raw H/B second moments and
product of inertia at IMPORT TIME (finds the rotation angle phi that zeroes the rotated product
of inertia, same rotation sense as `MidasOrientation.hpp`'s Beta formula) rather than adding a
product-of-inertia term to `kopruq::Section`/`Beam3D` itself -- avoids any core stiffness-
formulation change. Sanity-checked arithmetically (not against real data, see below): L50x5 raw
IH=IB=11.25 cm^4 (tabulated ~11.0), principal 17.91/4.59 cm^4 at phi=45 degrees (tabulated
~17.5/4.45). Also added: a node connected only by TRUSS elements has no rotational stiffness
source at all (documented in `Truss3D.hpp`), so RX/RY/RZ are now auto-restrained there
(disclosed via `report.assumptions`), matching MIDAS's implicit pin-jointed-truss convention.

**All 25 of model 20's elements now import without rejection** (previously 12/25). The model
still does NOT solve: a from-scratch eigenvalue diagnostic of the assembled free-DOF stiffness
matrix (not kept in the tree) found 3 genuine zero-eigenvalue modes. Root cause, confirmed by
direct structural reasoning and the numeric eigenvectors together: model 20 mixes continuous
BEAM chords (top: nodes 1-7 at Z=0; bottom: nodes 8-14 at Z=-0.8) with pin-ended TRUSS
diagonals/verticals, entirely within the Y=0 plane. TRUSS members carry zero out-of-plane
(global Y) stiffness by construction (axial-only, and none of them has a Y-component to their
own axis here). The model's only 2 supports (nodes 8, 14) are on the BOTTOM chord, so the bottom
chord's out-of-plane translation is pinned there -- but the TOP chord has no path to resist
global-Y translation at all (its own weak-axis bending stiffness is "free-free", unsupported at
either end, since the only coupling to the rest of the structure is via zero-Y-stiffness
trusses). This is a structurally genuine instability in how the KOPRUQ-SOLVER model is currently
assembled from this JSON, not a bug in the Iyz work above or in the T/box/I/channel
Iy-Iz-mapping convention (independently re-confirmed correct via the eigenvector inspection).

**Not resolved, deliberately -- flagged for the user rather than guessed:** whether MIDAS
implicitly restrains out-of-plane DOFs (UY, and likely RX/RZ) for a model whose entire geometry
lies in one plane (confirmed here: bounding box Y[0,0]) is a disclosed, but broad, modeling-
convention assumption that would affect every future purely-planar model, not just this one --
exactly the kind of architecture-level decision CLAUDE.md reserves for explicit user approval
rather than inferring from one sample. Model 20's L50x5/truss-restraint work is otherwise
complete; this is the sole remaining blocker for its full numerical verification.

## Model 20 fully solved and verified; "Truss Force" added as a comparable category (2026-11-19, commits `7353640`, `b1fb1e3`)

**Confirmed empirically:** the user re-solved model 20 in MIDAS as an explicit 2D model and
re-exported fresh JSON/Excel. Its `CONS` records are byte-identical to the original 3D-solved
export -- proving MIDAS's "2D" setting truly suppresses the 3 out-of-plane DOFs everywhere with
NO CONS footprint at all. `MidasJsonImporter.hpp` now detects a genuinely planar model (bounding
box exactly 0 along one global axis -- strict, not a near-planar heuristic) and auto-restrains
those 3 DOFs at every node, disclosed via `report.assumptions`.

Also found and fixed: `equalAngleSection`'s own idealized no-fillet area for L50x5 (4.75 cm^2)
measurably disagreed with the published EN 10056-1 tabulated value (4.80 cm^2) -- enough to fail
real displacement comparison. `resolveCatalogSection` now overrides area to the published value
for this specific catalog lookup (a raw DBUSER `vSIZE` input still has no such published number
to fall back on).

**Final model 20 verification (real MIDAS reference data):**
- Reaction(Global): **36/36 PASS**
- Displacements(Global): **252/252 PASS**
- Beam Force: 370/432 PASS (62 FAIL, all ~2-4% on bottom-chord members not directly loaded --
  investigated via an AsH shear-area variant without resolving it further; consistent with the
  idealized no-fillet HEB160/UPN100/L50x5 geometry's effect on force redistribution through a
  statically indeterminate truss-chord system; disclosed as a residual accuracy limitation, not
  a logic defect -- Reactions and Displacements both match exactly, so overall equilibrium and
  the global stiffness solution are confirmed correct)
- Truss Force (new category, see below): 54/78 PASS (24 FAIL, same residual, not a matcher bug)

**New verification category, per user request:** `ReferenceFileKind::TrussForce` +
`matchTrussForces` (`verification/ResultMatcher.cpp`) parse MIDAS's "Truss Force" export (a
single axial force per TRUSS element, as separate Force-I/Force-J columns in one row, unlike
Beam Force's per-end-row "Part" layout) and compare against the solver's own recovered
`local_end_forces[0]/[6]`. Wired through the GUI end-to-end mirroring Beam Force: a new
"Truss Force (Excel)" file-selector row, a new "Kafes Elemanı Kuvvetleri" results tab, and a new
"Truss Forces" sheet in the exported verification report. Full regression: 291/291 Release,
261/261 Debug, unchanged.

Model 20 (truss) is now complete, per the user's "19 spring, 20 truss, 21 combo" instruction.

## Model 21 (combo) — nested-combination logic independently verified; closed, STRK/SOCT not implemented (2026-11-19)

**Nested `CB` combination expansion, independently verified correct.** Model 21's own four
`LCOM-GEN` entries are the rigorous test this project had been waiting for since Phase 7G
(COMBO-3/COMBO-4 reference other combinations via `ANAL=="CB"`, not just primitive load cases).
Hand-derived (independently, NOT by running the importer first) the expected flat expansion:
```
COMBO-1 = FORCE-1(1.0) + FORCE-2(1.0) + TU(0.5)                      [primitive]
COMBO-2 = FORCE-1(1.0) + FORCE-2(2.0) + FORCE-3(3.0) + TU(1.0)        [primitive]
COMBO-3 = 1.0*COMBO-1 + FORCE-1(0.5) + TU(1.0)
        = FORCE-1(1.5) + FORCE-2(1.0) + TU(1.5)
COMBO-4 = 1.0*COMBO-1 + 2.0*COMBO-2 + 0.6*COMBO-3
        = FORCE-1(3.9) + FORCE-2(5.6) + FORCE-3(6.0) + TU(3.4)
```
The importer's actual output (diagnostic dump of `ModelInput::combinations`, not kept in the
tree) matched this **exactly**, term for term, confirming Phase 7G's recursive `expand()` logic
(`MidasJsonImporter.hpp`) is correct for genuine multi-level nesting (COMBO-4 depends on COMBO-3,
which itself depends on COMBO-1), not just the simple non-nested case models 06/10 could exercise.

**Everything else in model 21 imports cleanly:** 180 nodes, 188 beams (all `TYPE:"BEAM"`), 1
material (CONC, already supported), 12 supports, 16 nodal loads, 68 member loads, 262 thermal
loads -- zero rejections other than the 2 section shapes below.

**STRK/SOCT (16/188 elements, 8.5%) -- deliberately NOT implemented; this is the model's sole
remaining blocker for a full end-to-end numerical solve.** Investigated via web search (two
subagent fetches): `STRK` is MIDAS's code for "Solid Track" per one source (a stadium/racetrack
shape) but a different, equally-official-looking source's own shape-symbol table instead lists
`STRK` as "Solid Octagon" (the same name as `SOCT`) -- an internally inconsistent/unclear naming
even across what should be authoritative MIDAS documentation. Neither source gives the `vSIZE`
dimension order for either shape. `support.midasuser.com` (which likely has the real DBUSER
shape-dimension reference) returns HTTP 403 to automated fetches, consistent with every earlier
attempt to reach it this project has made. Guessing a geometric parametrization from the raw
`vSIZE` numbers alone (`STRK`: `[0.9, 2.0]`; `SOCT`: `[1.0, 2.0, 0.3, 0.2]`) would be fabricating
structural properties -- explicitly forbidden. Resolving this needs either the user's own access
to MIDAS's official DBUSER section reference, or a sample model whose MIDAS-reported section
area/Iy/Iz for one of these two shapes can be reverse-matched against a candidate parametrization
(neither available this session).

Model 21 (combo) is closed per the user's instruction, with its core subject (nested load
combinations) fully, independently verified; the STRK/SOCT gap is disclosed, not silently
skipped or guessed around.

## STRK/SOCT implemented via user-supplied MIDAS dialog screenshots; model 21 fully solves and tested (2026-11-19, commit `595b1ac`)

The user supplied MIDAS NX's own "Section Data" dialog screenshots for model 21's sections 4/5
(SEC4/STRK, SEC5/SOCT), directly confirming what no automated web attempt this session (or any
prior session) could reach: STRK is a stadium shape with dimensions H (=the two semicircular end
caps' diameter) and B (overall length); SOCT is NOT a regular octagon -- a rectangle H x B with
each of its 4 corners cut by a right triangle of INDEPENDENT legs a (horizontal) and b
(vertical).

Per the user's explicit instruction ("derive everything from general mechanics of materials...
we don't have to stick to MIDAS NX"), `solidTrack`/`solidOctagon`
(`MidasSectionLibrary.hpp`) derive every property from first-principles classical mechanics
(composite-shape decomposition + parallel-axis theorem), not fitted to any MIDAS-reported
number. `torsionConstantFEM`'s rectangle-union-only interface was generalized into a shared
`torsionConstantFEMCore` taking an arbitrary `inside(x,y)` predicate, so the stadium's curved
boundary and the octagon's chamfered corners can both reuse the same already-verified FDM
solver. Verified via two EXACT limiting-case checks (new file,
`tests/unit/midas_section_library_strk_soct_tests.cpp`): a degenerate track (B=H) matches
`solidCircle`'s own closed form exactly (Area, IH, IB; J within ~0.8% after tuning grid
resolution for a curved boundary); a degenerate octagon (a=b=0) matches `solidRectangle`'s own
closed form exactly, including its torsion dispatch.

**Model 21 now solves end-to-end for the first time.** Verified against real MIDAS reference
data: Beam Force 14505/18048 PASS (80.4%), Reaction 489/576 PASS (84.9%), Displacements
6998/8640 PASS (81.0%). This is NOT 100%, and the gap is disclosed, not minimized:
- The majority of residual failures are small (1-5%) discrepancies spread broadly across the
  structure, including on elements that do NOT use STRK/SOCT at all -- consistent with the
  newly-added 16 STRK/SOCT elements (previously rejected, now contributing real stiffness)
  shifting the overall force distribution in this highly indeterminate 180-node 3D frame, and
  with the STRK/SOCT formulas being reasonable, independently-derived approximations rather than
  MIDAS's own (unknown) exact internal values -- the same category of residual seen on model 20's
  idealized no-fillet catalog sections, now showing at the scale of a much larger, more
  interconnected structure.
- A distinct, unexplained anomaly: a chain of ~10 elements (nodes 51-58-69/70, all section 1/SB,
  NOT STRK/SOCT) shows a large (up to ~3.7x) DX displacement discrepancy specifically under
  COMBO-3 (which includes a TU thermal term), while the same chain's FORCE-1/2/3 and TU-only
  comparisons individually pass, and COMBO-4 (which also includes 0.6*COMBO-3) shows no such
  anomaly. Not investigated further this session (effort already substantial); plausible
  explanation is thermal-displacement accumulation along a long element chain amplifying a small
  per-element discrepancy, but this is a hypothesis, not a confirmed root cause -- flagged
  honestly as unresolved, not silently absorbed into the "idealized geometry" explanation above.

Full regression: 297/297 Release (291 + 6 new), 267/267 Debug (261 + 6 new). Model 21 is closed
per the user's instruction, with STRK/SOCT implemented and the model's full numerical behavior
tested and honestly reported -- not claimed as a clean PASS.

## Model 21 deep debug: two root causes separated -- one KOPRUQ defect fixed, one MIDAS reference inconsistency proven (2026-11-19)

Supersedes the "COMBO-3 DX anomaly ... COMBO-4 shows no such anomaly" paragraph above. That
COMBO-4 statement was WRONG: it came from a diagnostic that capped printed FAIL rows at 200 per
category. Exact, untruncated per-case counts show COMBO-4 has the same anomaly (819/1080 FAIL).

**1. KOPRUQ defect (FIXED, this session): thermal loads were dropped from load combinations.**
`LinearStaticSolver::solveCases` (and the separate rigid-link path `solveCasesWithRigidLinks`)
build each combination's combined load case by copying the factored nodal, member, point and
self-weight loads of its terms -- but NOT `thermal`. Combination displacements were correct
(superposition of case solutions), but recovered element end forces (f = k*u - f_eq) and support
reactions lacked the thermal fixed-end forces. Proof on model 21: KOPRUQ's own combination axial
forces violated superposition by 8160-39712 kN, and for all 1504 combination axial rows that
residual equalled the independently hand-computed missing force E*A*alpha*dT*sum(f) to within
6.4e-5 kN (CSV round-off); every other component/category superposed exactly. Fix: one shared
helper `appendFactoredLoads` (LinearStaticSolver.hpp) used by both paths, scaling
uniform/gradient delta_t by the factor (thermalLoadEffect is linear in each; depths unscaled).
New dedicated test file `tests/unit/thermal_combination_tests.cpp` (4 tests, hand-derived
expected values: fixed-fixed uniform N = E*A*alpha*f*dT incl. support reactions; fixed-fixed
gradient M = E*Iy*alpha*f*dT/h; series bars with mixed thermal + mechanical combination by hand
compatibility; rigid-link path), plus combination == sum(f * case) for every end force and
reaction. Built against the pre-fix HEAD header, all 4 tests FAIL (combination forces/reactions
exactly 0 instead of e.g. 3600 kN); with the fix all PASS.

Model 21 after the fix (new MIDAS run, `verification/models/21 Combo/`, analysis 16:20,
Displacements re-exported 16:28): Beam Force 14505 -> **15553/18048 (86.18%)**; combination
axial max |diff| 3.98e4 -> 127 kN. Displacements 6998/8640 (81.00%) and Reaction 489/576
(84.90%) unchanged (expected: displacements were never affected; no thermally loaded element
touches a support in this model). Tolerance criterion unchanged (|d| <= absTol + 1%*|ref|).

**2. MIDAS reference inconsistency (NOT a KOPRUQ defect, unresolved on the MIDAS side).**
Self-consistency check, MIDAS data only (each combination row rebuilt from the same file's own
primitive rows with the JSON's factors): Reaction and Beam Force are exactly consistent for all
4 combinations (residual <= 1e-9). Displacements(Global) is consistent for COMBO-1/2 but NOT for
COMBO-3/4 (168/180 nodes, max residual DX 4.2e-3 / 1.7e-2 m); least squares on the file itself
gives COMBO-3 = 2.0F1+1.0F2+1.0TU and COMBO-4 = 4.2F1+3.6F2+6.0F3+2.1TU instead of the
JSON/Reaction/BeamForce 1.5F1+1.0F2+1.5TU and 3.9F1+5.6F2+6.0F3+3.4TU. The user re-ran MIDAS
and re-exported: the new file is cell-for-cell identical to the old one, so this is a
reproducible MIDAS displacement-combination behavior for this nested-combination model, not a
stale export. Its cause is not known and is not guessed. These 1607 displacement FAILs (COMBO-3
788, COMBO-4 819) are reference-side; the 6 primitive/COMBO-1/2 cases are at 97.7-100%.

**3. Remaining Beam Force/Reaction residual (open).** Spread over all sections and load cases
(incl. primitives), no sign flips (0 of 8351 rows), per section/component median KOPRUQ/MIDAS
ratio 0.993-1.006 except SOCT torsion (median 1.028, p90 1.070). Consistent with section-
stiffness idealization differences (shear areas, J); root cause not proven, no change made.

Regression: Debug 271/271, Release 271/271, gui-release 301/301 (each previous count + 4 new).

## Model 21 full verification: nested-combo displacement proven MIDAS-side; every other residual traced to 4 material/section-property conventions (2026-11-19, research only, no production change)

**Nested vs flat combinations (user added COMBO-3F/COMBO-4F = explicit flat forms of COMBO-3/4).**
In MIDAS's own exports: Reaction and Beam Force give COMBO-3 == COMBO-3F and COMBO-4 == COMBO-4F
(max |diff| 3e-11); Displacements(Global) gives COMBO-3 != COMBO-3F (168/180 nodes, max 4.2e-3 m)
and COMBO-4 != COMBO-4F (max 1.7e-2 m), while COMBO-3F/4F are exactly consistent with the file's
own primitive rows. MIDAS's nested-combination displacement output is therefore inconsistent with
its own flat equivalent; KOPRUQ (which expands nested combinations exactly) matches the flat
forms: COMBO-3F 1073/1080, COMBO-4F 1037/1080 with current properties, 1080/1080 each in the
diagnostic experiment below.

**Property back-calculation from MIDAS's own data** (scratchpad tools `backcalc_stiff.py`,
`diag_bend21.cpp`; load cases without member loads, so end forces depend only on end
displacements): axial EA and torsional GJ from N = EA/L*du, T = GJ/L*dtheta; bending EI and
Timoshenko Phi by least squares on the 2-node Timoshenko relations (linear in a = EI/(L(1+Phi)),
c = Phi*a). Self-check: applied to KOPRUQ's own solution it recovers KOPRUQ's EI/As exactly
(residual ~1e-15). Findings (MIDAS value / KOPRUQ value):
- E (all sections, models 21 AND 18): 34077 MPa = EN1992-1-1:2004 Ecm = 22*(fcm/10)^0.3 for
  C35/45 (34077.15) rounded to 1 MPa. KOPRUQ uses the Table 3.1 rounded 34000 MPa. NOTE: the
  MidasMaterialLibrary.hpp header calls 22*(fcm/10)^0.3 and 9.5*(fck+8)^(1/3) "the same curve" --
  false (34.08 vs 33.28 GPa); the old model-04 evidence (2.13% deflection gap) is ambiguous
  between them because of shear deformation.
- A, Iy, Iz: exact for SB, SR, B, STRK, SOCT. J exact for SB, SR, B.
- J: STRK(H=0.9,B=2) 0.301992 vs 0.315001; SOCT(H=1,B=2,a=0.3,b=0.2) 0.379947 vs 0.433536.
  MIDAS's formula NOT identified (several closed forms tried, none match); not guessed.
- Shear areas: SB 5/6*A, SR 0.9*A exact. Box: Asy = 2*tf*B, Asz = 2*tw*H (exact on model 21
  B[1.5,1.0,0.1,0.2]: 0.400000/0.300000 AND model 18 B[3,5,0.5,0.5]: Asz 3.00000 -- KOPRUQ's
  Zhuravskii values are 0.37669/0.29205 and 2.98475; model 18 passed only within tolerance).
  T (model 18): Asy = 5/6*B*tf = 1.66667 (KOPRUQ 2.74809). STRK: Asy = Ls*H + 0.9*pi*R^2 =
  1.562555, Asz = (5/6)*Ls*H + 0.9*pi*R^2 = 1.397555 (single geometry). SOCT: Asy = Asz = 5/6*A
  = 1.566667 (single geometry).

**Diagnostic experiment** (scratchpad `diag_exp21.cpp`, properties overridden after import, never
production): with E=34077 MPa, the MIDAS-implied J for STRK/SOCT and the shear areas above, model
21 gives Beam Force 22560/22560, Reaction 720/720 (max rel diff ~6e-4, typically 1e-5), and
Displacements 1080/1080 for every case except the MIDAS-inconsistent nested COMBO-3/COMBO-4.
This proves KOPRUQ's stiffness assembly, thermal/member/nodal load vectors, combination logic and
force recovery reproduce MIDAS essentially exactly; all remaining model-21 residual is the 4
property conventions. Current production (unchanged): Beam Force 19351/22560 (85.78%), Reaction
606/720 (84.17%), Displacements 9108/10800 (84.33%). Changing E/shear-area/J conventions is a
formulation change awaiting user approval; STRK/SOCT J needs MIDAS's own section values.

## Model 21 revision (flat combinations, 2 more STRK/SOCT sections): EN Ecm formula + MIDAS section conventions implemented (2026-11-20)

User decisions (2026-11-20): (1) no nested load combinations in the verification models --
combinations are plain factored sums of load cases (the importer still expands nested CB terms
exactly; nothing removed); (2) concrete E from the EN1992-1-1 formula, not the Table 3.1 rounded
values. The user revised model 21 (flat COMBO-1..4; added SEC6 STRK H=1.2 B=3.0 and SEC7 SOCT
1.3x2.5 a=0.4 b=0.25) and exported MIDAS's own section properties (Section_prop.xlsx). All four
new reference exports are now exactly self-consistent (combination rows == factored primitive
rows); the former nested-combination displacement inconsistency is gone with the flat definitions.

Production changes (MidasMaterialLibrary.hpp, MidasSectionLibrary.hpp):
- Concrete Ecm = 22*((fck+8)/10)^0.3 GPa for every EN grade (C35/45: 34077.15 MPa). The Table 3.1
  lookup and the old 9.5*(fck+8)^(1/3) fallback (a different, older curve) are removed.
- Box (B) shear areas: AsH = 2*tw*H, AsB = 2*tf*B (MIDAS convention; exact on model 21 SEC3 and
  model 18; replaces the Zhuravskii quadrature). Bredt J unchanged (already exact vs MIDAS).
- STRK: J = Roark rectangle formula on the equal-area rectangle (A/H x H); AsH = 5/6*Ls*H +
  0.9*pi*R^2, AsB = Ls*H + 0.9*pi*R^2. Exact (6 digits) on SEC4 and SEC6 against MIDAS.
- SOCT: AsH = AsB = 5/6*A (exact on SEC5 and SEC7). J kept as the exact St. Venant FDM value:
  MIDAS's SOCT J (0.379948 for SEC5, 1.026500 for SEC7, ~12% below exact) matched no closed form
  tried (equal-area/bounding/decomposed rectangles, ellipse, A^4/Ip forms, exhaustive search of
  rectangle combinations: best 0.5% off, coincidental) -- not guessed.
Tests: new tests/unit/midas_section_midas_conventions_tests.cpp (7 tests). Existing tests whose
expected values encoded the superseded requirements were updated explicitly (not to force a
pass; the requirement changed by user decision): midas_material_library_tests (3 tests,
cube-root/Table-3.1 expectations -> EN formula 34077.146 MPa; two renamed), the two box
shear-area expectations (midas_section_shear_area_tests, midas_section_library_tests;
Zhuravskii 2.188/4.476 -> 2*tw*H=2.0 / 2*tf*B=5.0, one renamed), and the J line of the STRK
degenerate-case test (circle J within 1% -> MIDAS convention 0.0551904; A/I/As circle checks kept).

Results, all models (static categories, same untruncated diagnostic, before -> after): no
regression anywhere; model 17 Beam Force 817 -> 819; model 21 Displacements 8544 -> 8611/8640
(99.66%), Beam Force 15598 -> 17367/18048 (96.23%), Reaction 475 -> 563/576 (97.74%). Every other
model's counts identical. Diagnostic experiment (not production): overriding ONLY the two SOCT J
values with MIDAS's gives model 21 8640/8640, 18048/18048, 576/576 -- the remaining 723 FAILs are
entirely the SOCT torsion-constant convention. Models 02/03/04/08/09 (pre-existing FAILs in this
diagnostic) and 10/22 (do not solve: 'element property reference missing') are unchanged and
were not investigated in this milestone.

Regression: Debug 278/278, Release 278/278, gui-release 308/308.

## SOCT torsion-constant convention identified and implemented -- model 21 fully verified (2026-11-20)

The user added a controlled series of SOCT sections to model 21 for this purpose (SEC7 changed to
[H,B,a,b]=[1.3,2.5,1.0,0.5]; new SEC8 [1.3,2.5,~0,~0], SEC9 [1.3,2.5,0.45,~0], SEC10
[1.3,2.5,~0,0.5]; MIDAS was given 1e-5 for a "zero" leg) and re-exported JSON, results and
Section_prop. Key observation: SEC9/SEC10 have the rectangle's area and inertias in MIDAS, yet a
lower J -- MIDAS's J depends on a and b directly, not on the cut geometry. Effective-rectangle
inversion gave B' = B - a/4 (SEC9) and H' = H - b/4 (SEC10), leading to

    J_SOCT = Roark solid-rectangle formula on (B - a/4) x (H - b/4)

confirmed to 6 significant digits on SIX geometries, each J back-calculated per element
(J = T*L/(G*dtheta), G from E=34077 MPa, nu=0.2) from MIDAS's own displacement and beam-force
exports, and consistent with Section_prop (4 digits): SEC5 0.379948, SEC7 0.818870, previous-
revision SEC7 [1.3,2.5,0.4,0.25] 1.026500, SEC8 1.234699, SEC9 1.153053, SEC10 0.953202.
This is MIDAS's convention, not exact elasticity: vs the exact St. Venant value (independent
Python FDM, h=H/300) it is ~13% low for moderate chamfers (SEC5 0.379948 vs 0.437099) and ~28%
high for the large one (SEC7 0.818870 vs 0.637362). The exact FDM function
(detail::torsionConstantFEMChamferedRect) is kept and tested.

Production change: MidasSectionLibrary.hpp solidOctagon() J only (shear areas, A, I unchanged).
Tests: new tests/unit/midas_section_soct_torsion_tests.cpp (9 tests: six MIDAS geometries,
formula literals at 1e-9 and MIDAS 6-digit values at 5e-6 relative; a=b=0 rectangle; exact
function still available; dispatch). Existing test MidasSectionConventions.
SoctTorsionIsExactStVenantNotMidas (written in the previous milestone, encoding "SOCT J = exact")
renamed SoctExactStVenantTorsionFunctionStillVerified and now calls the exact function directly
(same values, same 1.5% tolerance) -- requirement change, not a forced pass.

Model 21 (new data, untruncated, |d| <= absTol + 1% |ref|), before -> after:
Displacements 8573 -> 8640/8640 (100%), Beam Force 16562 -> 18048/18048 (100%), Reaction
539 -> 576/576 (100%); max relative deviations 6.3e-3 / 7.6e-3 / 4.3e-3. Model 21 is fully
verified against MIDAS for all static categories. All other models (01-20, 22): counts identical
to the previous milestone (no regression; 10 and 22 still do not solve -- next task).

Regression: Debug 287/287, Release 287/287, gui-release 317/317.

## Model 10 (3D frame): MIDAS DBUSER "H" section implemented -- model 10 fully verified (2026-11-20)

Root cause of "element property reference missing": model 10's section 3 ("PROFIL") is a MIDAS
DBUSER shape "H", vSIZE [1.0, 0.5, 0.02, 0.03, 0.6, 0.05, 0, 0], which the importer rejected
(not implemented), leaving its 4 elements without a section. Fresh MIDAS exports (the user re-ran
model 10, files dated 2026-11-20 09:49-09:51) were copied to build/gui-release/m10r1.

Before implementing, every property MIDAS uses for this section was back-calculated per element
from MIDAS's own results (independent Python, scratchpad backcalc_all.py; nodal-load cases
FORCE-2/FORCE-3; 2-node Timoshenko fit, residual ~1e-7; the tool was first checked against the
known SB/SR sections of the same model): A 0.06340000, IH 0.010876745, IB 0.001213113,
J 0.000032060, AsH 0.020000, AsB 0.037500. These fix the vSIZE order [H, B1, tw, tf1, B2, tf2]
(top flange B1 x tf1, bottom flange B2 x tf2) and MIDAS's conventions exactly:
  J = (B1 tf1^3 + B2 tf2^3 + (H - (tf1+tf2)/2) tw^3)/3  (clear-height 0.000031953 and full-height
      0.000032167 web variants do not match); AsH = tw*H; AsB = 5/6 (B1 tf1 + B2 tf2).
Evidence is one geometry (the only DBUSER H available). Nonzero fillet radii r1/r2 are rejected
with a clear message (MIDAS's treatment unverified). The catalog path (HEB160 -> iSection) is
unchanged.

Production change: MidasSectionLibrary.hpp, new unequalFlangeISection() + "H" dispatch.
Tests: new tests/unit/midas_section_h_dbuser_tests.cpp (6 tests: model-10 values, independent
literals at 1e-9 and MIDAS back-calc at 5e-6; symmetric-I closed form; flange-swap invariance;
dispatch; rejections). Existing MidasSectionLibrary.RejectsUnsupportedShapeAndInvalidDimensions
still passes unchanged: its "H" with 2 vSIZE values is now rejected as too few values instead of
as an unsupported shape.

Model 10 (untruncated, |d| <= absTol + 1% |ref|): before -> did not solve; after Displacements
1104/1104, Beam Force 2304/2304, Reaction 96/96 (100%); max relative deviations 4.6e-6 / 1.1e-6
/ 6.1e-8. All other models (01-09, 11-21): counts identical (model 22 still does not solve --
separate cause, not yet investigated).

Regression: Debug 293/293, Release 293/293, gui-release 323/323.

## Model 10 revision 2: second DBUSER H, AISC10(US) catalog I-shapes, S355 steel -- fully verified (2026-11-20)

The user revised model 10 (files 2026-11-20 11:07-11:09, copied to build/gui-release/m10r2): six
sections (SB, SR, DBUSER H "PROFIL-1" [1.0,0.5,0.02,0.03,0.6,0.05], DBUSER H "PROFIL-3"
[0.9,1.0,0.03,0.03,1.0,0.03], catalog "AISC10(US)" W36X247 and HP14X89) and a second material,
S355 (EN05), used by every H member.

Per-element back-calculation from MIDAS's own results (scratchpad backcalc_all.py, nodal-load
cases) gives:
- S355: E = 210000 MPa, nu = 0.3 (the importer's existing EN1993 values) -- confirmed by the
  DBUSER H members, whose geometry is known, reproducing A and J exactly with these E, G.
- PROFIL-3 (second DBUSER H geometry): A 0.0852, IH 0.01283976, IB 0.00500189, J 0.000025830,
  AsH 0.027, AsB 0.050 -- identical to unequalFlangeISection(), so the DBUSER H convention is now
  confirmed on two geometries (unequal and equal flanges).
- W36X247 / HP14X89: MIDAS uses the tabulated A, Ix, Iy, J (fillets included; not recomputable from
  the plate dimensions, e.g. W36X247 strip-sum J 33.1 in^4 vs tabulated 34.7) and shear areas
  AsH = d*tw, AsB = 5/6*2*bf*tf (the DBUSER H convention).
Source for the tabulated values: AISC Shapes Database (16th ed.), rows W36X247 / HP14X89 as
packaged in steelpy 1.1.1 (the official aisc.org download refused automated fetches, HTTP 403).
All six properties of both shapes agree with MIDAS's back-calculated values to that
back-calculation's printed precision (largest relative difference 1.8e-4, J of HP14X89 printed
to 4 digits).

Production change: MidasSectionLibrary.hpp -- aiscTabulatedISection() and an "AISC10(US)" branch in
resolveCatalogSection() for exactly these two shapes (any other AISC shape or AISC edition is
rejected with a clear message; nothing is estimated). UNI catalog unchanged.
Tests: new tests/unit/midas_section_model10_r2_tests.cpp (4 tests).

Model 10 revision 2 (untruncated, |d| <= absTol + 1% |ref|): before -> did not solve (AISC10(US)
rejected); after Displacements 1104/1104, Beam Force 2304/2304, Reaction 96/96 (100%). Largest
relative deviation on significant values: displacements (|ref| > 1e-4) 2.6e-5, forces/reactions
(|ref| > 10 kN) 2.0e-5 -- consistent with tabulated-vs-MIDAS-database rounding of the catalog
properties; the larger maxRel figures (2.2e-3) are on near-zero entries (absolute 2.7e-8).
All other models (01-09, 11-21, model 10 revision 1): counts identical.

Regression: Debug 297/297, Release 297/297, gui-release 327/327.

## Model 22: temperature gradient (GTMP), PSC 1-cell box, 2D FE section analysis, STYP -- 2026-11-20

User-approved tasks: (1) import of MIDAS beam temperature gradients, (2) MIDAS PSC "1CEL" box
section, (3) a 2D finite-element cross-section analysis module. Model 22 exports copied to
build/gui-release/m22r1. Sources: the user saved the MIDAS CIVIL NX online-manual pages
("Temperature Gradient", "Section Properties - PSC", "Section Properties - DB/User", "Sectional
Property Calculator", "Beam Section Temperatures") to C:\_ZHarput_Data\MIDAS NX_ONLINE HELP (read
only), and supplied MIDAS's Section Data / Section Properties screenshots for model 22's box.

Gradient (GTMP, MidasJsonImporter.hpp). Manual: TZ = T2z - T1z between the outermost faces, T2z
on the +z face; M = alpha E I dT / h; "Use Section Hz/Hy" = the section's overall depth / width.
KOPRUQ's gradient_*_delta_t is the opposite difference (ThermalLoads.hpp: a positive value bends a
cantilever toward +z), so gradient_z_delta_t = -TZ, gradient_y_delta_t = -TY. Proven from MIDAS's
own TG results: per-element back-calculation with the gradient fixed-end moments of either sign
-- only this sign reproduces EIy (13.67209, = TU-case value) and EIz (68.70453, = MIDAS's Izz),
with h = 3.15 / 10.0 (the box's full depth / width). Physical tests: a hot top face curls a
cantilever tip down by exactly kappa L^2/2; a hot +y face moves it to -y. Plate gradients (TYPE
!= 1) and missing depths are rejected. ResolvedSectionProperties now carries extentH/extentB.

PSC 1CEL (MidasSectionLibrary.hpp, psc1CellSection). Geometry from the manual's PSC-1CELL guide
diagram; JSON array order = MIDAS API order (verified for every nonzero entry against the dialog).
A, centroid, Iy, Iz by exact polygon integration: A 10.58, Iyy 13.6720934, Izz 68.7045333,
centroid 1.326008 below the top -- identical to MIDAS's Section Properties table. MIDAS's J and
shear areas are NOT closed-form: the manual's Bredt note gives J 19.73 (MIDAS 27.92024) and a
Zhuravskii strip integral gives Asz 5.12 (MIDAS 3.681402). An independent Python 2D FE analysis
reproduces MIDAS's three values on a coarse mesh (h = 0.1: J 27.93, Asy 5.434, Asz 3.675) and
converges below them (h = 0.0125: J 27.85023, Asy 5.421686, Asz 3.653751) -- MIDAS evaluates them
on its own section mesh. Joints on, 2CEL, unsymmetric, user stiffness, small holes: rejected.

2D FE section analysis (new include/kopruq/sections/SectionFE.hpp, own implementation): Bowyer-
Watson Delaunay mesh of boundary samples + interior triangular lattice, conforming boundary
recovery by sub-segment splitting, meshed area checked against the exact polygon area; linear
triangles; torsion by the warping function (multiply connected sections need no extra
condition); shear areas by Pilkey's shear functions (nu = 0 for MIDAS sections, which are
material independent). Verified: rectangle J vs Saint-Venant series (0.05% at h = 0.025,
converging from above), rectangle shear area 5/6 A, nu = 0.2 rectangle vs independent Python,
solid circle J = pi r^4/2 and 6/7 A, circular tube J = pi (R^4 - r^4)/2 and Cowper's area,
model 22 box vs independent Python (within 1.2e-4).

STYP (structure type, MidasJsonImporter.hpp) -- found while verifying model 22, a pre-existing
importer gap: STYP was ignored. Model 22 exports STYP = 1 (X-Z plane) and MIDAS's results have
DY = RX = RZ = 0 at every node in every case (with constraint reactions at non-CONS nodes); model
20, re-solved by the user as a MIDAS 2D model, also exports STYP = 1; all 3D models export 0.
STYP = 1 now restrains UY, RX, RZ at every node; other nonzero values are reported, not guessed.

Model 22 results (untruncated, |d| <= absTol + 1% |ref|): before -> did not solve; after
Displacements 1419/1422, Beam Force 2617/2808, Reaction 205/216; the TG (gradient) case is 100%
in all three categories. Remaining differences fully explained by diagnostic overrides (not
production): with MIDAS's T-section convention for the 4 cap-beam elements (section "BASLIK",
T [2,2,1,1]: J = Roark's T-section formula, 1.052851562 vs MIDAS back-calc 1.052851615; Asz =
tw*H = 2.0 from back-calculation; Asy = 5/6*B*tf as seen in model 18) -> 1422/1422, 2804/2808,
216/216; additionally with MIDAS's own mesh-dependent PSC J/Asy/Asz -> 100% in all categories
(max rel 4e-6). The 4 rows left by the converged PSC values are deck Moment-y values near a
zero crossing in TU (122 and 6 kNm, abs diff 6 kNm, deck moments up to 3e4 kNm). The T-section
convention is NOT implemented in this milestone (not in the approved scope; next task).
All other models: counts identical (01-21, model 10 revisions 1 and 2).

Existing tests whose meaning shifted (pass unchanged): MidasJsonImporter.GtmpIsRejectedExplicitlyWhenPresent
(its GTMP item has no depth and no USE_HZ -> now rejected for a missing depth, still no load
fabricated). New tests: tests/unit/section_fe_tests.cpp (7), tests/unit/midas_psc_gradient_styp_tests.cpp (10).

Regression: Debug 314/314, Release 314/314, gui-release 344/344.

## MIDAS DBUSER T-section convention -- models 17, 18, 22 (2026-11-20)

User-approved next task. tSection() (MidasSectionLibrary.hpp) now uses MIDAS NX's own DBUSER T
convention (geometry A/IH/IB unchanged):
- J = Roark's T-section torsion formula without fillet (K1 flange + K2 web + alpha D^4, new
  detail::roarkTeeTorsion): 1.0528515625 for T [2,2,1,1]; MIDAS back-calculated (model 22, cap-beam
  torsion in TU) 1.052851615. Superseded: strip sum 1.0 (the exact St. Venant value would be 0.9276).
- AsH = tw*H = 2.0 (MIDAS model 22: 2.0000); AsB = 5/6*B*tf = 1.66667 (MIDAS model 18 POINT case:
  1.666666-1.666669). Superseded: Zhuravskii 2.345 / 2.748. Same web / 5/6-flange rule as the DBUSER
  H section.
Evidence is the one T geometry used in the verification models (formula also checked on a second
geometry against an independent Python evaluation). Existing test
MidasSectionLibrary.TSectionMatchesHandComputation_Baslik had its J/AsH/AsB expectations updated
explicitly (requirement change, documented in the test); new tests/unit/midas_section_tee_tests.cpp (3).

Results (untruncated, same tolerance):
- Model 22: Displacements 1419 -> 1422/1422, Beam Force 2617 -> 2804/2808, Reaction 205 -> 216/216.
  The 4 remaining rows are the TU deck Moment-y values near zero crossings (122 / 6 kNm, abs diff
  6 kNm) caused by MIDAS's mesh-dependent PSC J/shear areas (with MIDAS's own PSC values all 100%).
- Model 17 static FORCE: Beam Force 819 -> 936/936, Reaction 43 -> 48/48 (Displacements 474/474).
  Model 17 dynamic (automated, all rows, new scratchpad tool diag_rs17): spectrum displacements
  944 -> 948/948, spectrum reactions 94 -> 96/96, eigen frequencies (1%) 38 -> 40/40 -- the two
  modes previously accepted as a 1.1-1.3% residual were caused by the T-section stiffness.
- Model 18: unchanged (already 100%). All other models: unchanged.

Regression: Debug 317/317, Release 317/317, gui-release 347/347.

## Task 3: pre-existing FAILs in models 02/03/04/08/09 -- root causes found and fixed (2026-11-20)

Two pre-existing importer defects, both silent:

A. Planar-model heuristic (removed). Since 2026-11-19 the importer auto-restrained the 3
   out-of-plane DOFs at every node whenever the node bounding box had zero extent along one global
   axis. MIDAS solves such STYP = 0 models as full 3D: model 02 (cantilever on the X axis, tip FY)
   came out with every displacement zero (MIDAS: DY, RX, RZ nonzero); model 09 (L-shaped cantilever
   in the X-Y plane, FZ) likewise. The model that motivated the heuristic (model 20) exports
   STYP = 1, now handled explicitly -- the plane restraint comes from STYP only. No model became
   singular (the 3D supports of models 04/08/12/16 already restrain torsion/lateral DOFs).
B. Section offset ignored. Models 02/03/04/08 use OFFSET_PT "CT" (node at the top fiber, member
   axis at the centroid H/2 below): MIDAS shows the resulting axial DX at the nodes under bending
   and torsion RX under a horizontal load. "CT" is now a rigid member-end offset (existing
   Beam3DConnection offset1/offset2, Offsets.hpp) of -topFiber along the member's local z;
   ResolvedSectionProperties carries topFiber (H/2 for vertically symmetric shapes, the computed
   centroid depth for T, unequal-flange H and PSC). Other offset points (CB, L*/R*) and any
   user-defined offset part are rejected, not guessed (no MIDAS data to verify them).

Results (untruncated, same tolerance), before -> after:
- 02: Displacements 18 -> 30/30, Beam Force 25 -> 48/48, Reaction 4 -> 6/6
- 03: Displacements 26 -> 30/30 (forces/reactions already 100%)
- 04: Displacements 92 -> 108/108 (forces/reactions already 100%)
- 08: Displacements 336 -> 396/396 (forces/reactions already 100%)
- 09: Displacements 33 -> 54/54, Beam Force 48 -> 96/96, Reaction 6 -> 12/12
Max relative deviation on significant values: displacements 4.3e-6 (= MIDAS rounding E to
34077 MPa), forces/reactions <= 6e-8. Every other model unchanged (01, 05-07, 10-12, 16-22).
New tests: tests/unit/midas_offset_planar_tests.cpp (4; DY = PL^3/3EIz on a STYP-0 planar model;
CT: DX = +P L^2 e / 2EIy, RX = -P e L / GJ, both derived from beam theory; rejections).

Regression: Debug 321/321, Release 321/321, gui-release 351/351.

## Models 16 and 17: response-spectrum and eigen results compared in full (2026-11-20)

Automated comparison of every MIDAS response-spectrum row (CQC, lumped mass; peak magnitudes,
|KOPRUQ| vs |MIDAS|, |d| <= absTol + 1% |ref|) and the eigen frequencies (1%), scratchpad tool
diag_rs17 (ResponseSpectrumSolver / ModalSolver on the imported model). Previously model 16/17
spectrum results had only been spot-checked by hand (8 reaction values). No code change.
- Model 16 (SPECX, 20 modes): displacements 234/234 (max rel 7.2e-5), reactions 24/24 (8.6e-5),
  beam forces 454/456, eigen 20/20. The 2 FAIL rows are the same value: element 88 J / 89 I,
  Moment-y at node 98 (mid-height of the X = 40 pier) -- KOPRUQ 15.97, MIDAS 16.60 kNm (abs 0.62).
  The pier works in double curvature and the dominant mode's moment crosses zero there (|My| along
  the pier: 2896, 1438, 16.6, 1452, 2873 kNm; model maximum 10516): the value is the CQC residue of
  the other modes, extremely sensitive. abs diff = 0.04% of the adjacent values. Reported as FAIL
  under the per-row criterion; not tolerated away.
- Model 17 (SPECX, SPECY, 40 modes): displacements 948/948, reactions 96/96, beam forces
  1872/1872 (max rel 3.8e-3), eigen 40/40.

## Models 13-15 (eigen) re-tested -- all 100% (2026-11-20)

GUI matcher (verification::matchEigenvalueFrequencies: frequency and angular frequency per mode,
1%) on ModalSolver (lumped mass, the model's own EIGV mode count), scratchpad tool diag_modal,
built twice: against commit 6d7573b (before the planar heuristic was removed) and the current tree.
No code change in this step.
- 13 Eigen-1 (planar X-Z frame, STYP 0, 20 modes): before 0/40 (frequencies 160-410% off: the
  removed zero-extent heuristic suppressed every out-of-plane mode, so MIDAS's 3D mode sequence
  could not match) -> now 40/40, max rel 3.5e-5.
- 14 Eigen-2 (planar, STYP 0, 30 modes): before 0/60 (6-20% off, same cause) -> now 60/60,
  max rel 3.7e-5.
- 15 Eigen-3 (3D, 1630 nodes, 100 modes): 200/200 before and now, max rel 1.3e-4.
- Same tool on models 16/17: 40/40 (3.5e-5) and 80/80 (3.5e-5).
The "isolated small mode divergence" previously noted as an accepted residual (models 13-15 and
17) no longer exists: model 17's came from the T-section stiffness, models 13/14's (as far as the
current data shows) from the planar heuristic introduced 2026-11-19.

## GUI: response-spectrum comparison added; GUI rebuilt with all of this session's work (2026-11-20)

All solver/importer work of this session lives in header-only code that the CLI (and therefore the
GUI, which runs kopruq_solver_cli) is built from; build/gui-release was rebuilt (CLI and
kopruq_verification_gui.exe). New in the GUI:
- verification::appendSpectrumCases (ResultMatcher): merges the CLI `spectrum` results into the
  comparison under MIDAS's "<case>(RS)" names, flagged peak_magnitude; for those cases the
  Displacement/Reaction/Beam Force matchers compare |KOPRUQ| with |MIDAS| (no beam-force sign
  convention). Ordinary cases unchanged.
- MainWindow: after a successful static solve, if the MIDAS model requests response spectra
  (SPLC), a `spectrum` CLI run is chained (mass type = the existing "Lumped Mass (MIDAS)" check box,
  whose tooltip now says so), then the modal run if an Eigenvalue Mode file is given. A failed
  spectrum run only leaves the (RS) rows N/A.
- New tests: tests/unit/result_matcher_spectrum_tests.cpp (3, GUI build only).
End-to-end check of the GUI path (real CLI solve + spectrum --mass-type lumped, merged, GUI
matchers; scratchpad tool gui_pipeline): model 16 SW 100% and SPECX(RS) 234/234, 24/24, 454/456;
model 17 FORCE, SPECX(RS), SPECY(RS) all 100%; models 02, 10, 21 100%; model 22 100% except TU
beam force 932/936 -- identical to the diagnostic-tool results above.

Regression: gui-release 354/354, Debug 321/321, Release 321/321.

## Model 22 re-exported without the eigenvalue request (2026-11-20)

The user removed EIGV from model 22 (a gradient-temperature model) and re-exported JSON and results
(16:08-16:11; workspace build/gui-release/m22r2). JSON diff vs the previous export: EIGV only.
Re-test (diagnostic tool and the GUI path -- real CLI solve + GUI matchers), identical results:
Displacements 1422/1422, Reaction 216/216, Beam Force 2804/2808 (TG 936/936, UNIFORM 936/936,
TU 932/936). The 4 TU rows are unchanged: deck Moment-y at zero crossings (el 13 J / 14 I:
122.14 vs 127.93 kNm; el 22 J / 23 I: 5.92 vs 7.79 kNm), the PSC mesh-dependence documented above.
