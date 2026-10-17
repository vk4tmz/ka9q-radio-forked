"""Source-level, hardware-free guardrails for experimental RX888 VHF tuner AGC.

Run from repo root:
  python3 -m unittest discover -s tests -p 'test_rx888_vhf_agc_source.py' -v
This is not a substitute for compiling radiod or hardware validation.
"""
from pathlib import Path
import re
import unittest

SOURCE = (Path(__file__).resolve().parents[1] / "src" / "rx888.c").read_text()


class VhfAgcSourceContracts(unittest.TestCase):
    def test_mode_is_explicit_and_manual_by_default(self):
        self.assertIn('"vhf-gain-mode", // manual (default)', SOURCE)
        self.assertIn('config_getstring(dictionary,section,"vhf-gain-mode","manual")', SOURCE)
        self.assertIn('strcasecmp(vhf_gain_mode,"tuner-auto") == 0', SOURCE)
        self.assertIn('Invalid vhf-gain-mode', SOURCE)

    def test_hardware_agc_masks_only_lna_mixer_mode_bits(self):
        self.assertIn('r828_write_byte_mask(sdr,0x05,0x00,0x10);', SOURCE)
        self.assertIn('r828_write_byte_mask(sdr,0x07,0x10,0x10);', SOURCE)

    def test_tuner_auto_is_applied_after_manual_gain_and_fixed_if(self):
        pos = SOURCE.index('char const *vhf_gain_mode =')
        end = SOURCE.index('frontend->lock = true;', pos)
        block = SOURCE[pos:end]
        self.assertLess(block.index('rx888_set_att(sdr,49.6f - vhf_rf,true);'),
                        block.index('if(tuner_auto){'))
        self.assertLess(block.index('rx888_set_gain(sdr,vhf_if,true);'),
                        block.index('if(tuner_auto){'))
        self.assertIn('frontend->rf_agc = false;', block)

    def test_hf_feedback_loop_excludes_vhf(self):
        self.assertRegex(SOURCE, r'frontend->rf_agc\s*&&\s*!\(sdr->undersample == 1 && frontend->frequency != 0\)')


    def test_normal_vhf_retune_does_not_directly_change_lna_mixer_mode(self):
        # Source regression only: the full method may invoke other helpers.
        start = SOURCE.index("static double rx888_set_tuner_frequency(struct sdrstate *sdr,double f){")
        end = SOURCE.find("\nstatic ", start + 10)
        block = SOURCE[start:end if end != -1 else None]
        for reg in ("5", "7", "0x05", "0x07"):
            self.assertNotRegex(
                block, rf"r828_write_byte(?:_mask)?\(sdr,\s*{reg}\s*,"
            )

    def test_existing_manual_control_can_override_hardware_auto(self):
        # Explicit documented limitation: manual RF writes set manual gain bits.
        pos = SOURCE.index("static void rx888_set_att(struct sdrstate *sdr,float att,bool vhf){")
        end = SOURCE.index("static void rx888_set_gain(struct sdrstate *sdr,float gain,bool vhf){", pos)
        block = SOURCE[pos:end]
        self.assertIn("r828_write_byte_mask(sdr,0x05,0x10,0x10);", block)
        self.assertIn("r828_write_byte_mask(sdr,0x07,0x00,0x10);", block)


if __name__ == "__main__":
    unittest.main()
