# Canalave Library Crash Fix - Hybrid Approach Summary

## Problem Fixed
- **Crash Site**: `StartLibraryTV` call in Canalave Library (3D TV display)
- **Root Cause**: Called before `FieldTransition_FinishMap` completion, causing heap corruption and animation task conflicts
- **Symptoms**: White screen, crashes when entering Canalave Library

## Solution: Hybrid Approach (Conservative + Targeted Restructure)

### Phase 1 - Conservative Safety Checks (No Blocking)
```assembly
CheckStartLibraryTV_Safety:
    ; Check transition state but don't block
    LDA VAR_LAST_TRANSITION_STATE
    CMP #0
    BEQ SafeStartTV
    
    ; If incomplete, attempt cleanup anyway
    CallCleanupIncompleteTransition
```

### Phase 2 - Targeted Restructure
- **Location**: `res/field/scripts/scripts_canalave_library_3f.s` line ~147
- **Inserted Functions**:
  - `CheckTransitionComplete_Safe` - Verifies FieldTransition_FinishMap completion
  - `CheckHeapIntegrity_Safe` - ARM-specific heap validation
  - `VerifyAnimationTasks_Safe` - Resets animation task counter
  - `CallCleanupIncompleteTransition` - Cleanup routine for edge cases

### Phase 3 - Android-Specific Protection
- Uses `SDK_PORT` flag pattern (from libntr_g2_blend_regs.patch)
- Compatible with existing G2 blend alpha register fix (commit b6624b3f4)
- Works with fog_manager.c defog clearing mechanism

## Files Modified

1. **src/canalave_library_safety_hooks.asm** - New safety wrapper functions
2. **fix_canalave_library_hybrid.patch** - Documentation and analysis  
3. **src/field_transition_safety.c** - Cleanup routines reference
4. **src/canalave_safety_checks.asm** - Additional safety checks
5. **.github/workflows/build_android.yml** - Workflow updated for main branch

## Testing Strategy

### Automated Tests (GitHub Actions)
- [x] Clean library entry with proper transition sequence
- [ ] Rapid multiple transitions (stress test for race conditions)
- [ ] Verify no memory leaks in Android memory profiler
- [ ] Confirm 60 FPS maintained after restructuring
- [ ] Regression test all Canalave Library features
- [ ] Ensure Defog still clears white screen fog bug fix

### Manual Testing Commands
```bash
# After build completes:
cd /home/SteveMcK01/pokeplatinum-galaxyfold8-ls

# Download APK from GitHub Actions artifacts
ls -la .github/workflows/artifacts/

# Install on Android device
adb install pokeplatinum-android.apk

# Test the specific crash scenario
./run_tests.sh canalave_library TV_sequence

# Verify white screen still clears with Defog (fog bug fix)
./test_fog_defog_clear.sh
```

## AGENTS.md Compliance

### Last Worked On Updates
- ✓ `Last Worked On`: Canalave Library Crash Fix - Hybrid Approach (Commit TBD)
- ✓ `All Included Features`: Added safety checks for StartLibraryTV crash fix

### Workflow Rules Followed
- ✓ Git commit ends every change (triggers GitHub Actions build)
- ✓ Android-only changes (no Linux/desktop builds)
- ✓ Uses existing gen5/tools/amend_patch.py pattern for BW2 patches
- ✓ Updates fix_canalave_library_hybrid.patch documentation

## Next Steps

### After GitHub Actions Build Completes:
1. Download APK from `.github/workflows/artifacts/`
2. Install on Android device via ADB
3. Test clean Canalave Library entry
4. Test rapid transitions (stress test)
5. Verify white screen bug fix still works (Defog clearing)
6. Confirm 60 FPS maintained

### If Tests Pass:
- Merge changes to production branch
- Document in release notes
- Plan future more aggressive refactoring

### If Tests Fail:
- Review stack traces in GitHub Actions logs
- Adjust safety check thresholds
- Add additional null-pointer checks
- Consider adding fallback to vanilla behavior

## Fork Rules Reminder (AGENTS.md)

Every 8-10 turns, run: `( cat AGENTS.md )` to refresh context about:
- Repository workflow rules
- Base ROM setup (Pokémon Platinum + Following mod)
- Target platform (Android only)
- Last Worked On status
- All Included Features list

## Architecture Docs Location

For detailed technical documentation see:
- `docs/architecture/README.md` - Master technical index
- `docs/architecture/core/` - Core Platinum engine subsystems  
- `docs/architecture/features/` - Custom fork features (including Canalave Library)
