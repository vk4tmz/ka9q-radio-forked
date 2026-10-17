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


if __name__ == "__main__":
    unittest.main()
