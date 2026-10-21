import copy
import tempfile
import unittest
from pathlib import Path
import pipeline
ROOT = Path(__file__).resolve().parents[2]
class ContentTests(unittest.TestCase):
    def test_complete_matrix_and_original_identity(self):
        audit = pipeline.audit(ROOT)
        self.assertEqual(len(audit['sources']), 55)
        self.assertEqual(len(audit['coverage']), 550)
        self.assertEqual(sum(p['status'] == 'original' for p in audit['coverage']), 55)
        self.assertEqual(sum(p['status'] == 'blocked' for p in audit['coverage']), 495)
        self.assertEqual(len({s['id'] for s in audit['sources']}), 55)
        self.assertEqual(audit, pipeline.audit(ROOT))
    def test_source_change_detected(self):
        baseline = pipeline.audit(ROOT)
        altered = copy.deepcopy(baseline)
        altered['sources'][0]['sha256'] = 'modified'
        with self.assertRaisesRegex(ValueError, 'source drift'):
            pipeline.verify_baseline(altered, baseline)
    def test_unknown_mapping_rejected(self):
        source = pipeline.audit(ROOT)['sources'][0]
        with self.assertRaisesRegex(ValueError, 'unmapped'):
            pipeline.validate_profile(source, {'target': 'Forest', 'mappings': {}})
    def test_refuse_manual_output_and_deterministic_write(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'variant.mroom'
            digest = pipeline.write_guarded(path, b'first', None)
            self.assertEqual(digest, pipeline.write_guarded(path, b'first', digest))
            path.write_bytes(b'manual edit')
            with self.assertRaisesRegex(ValueError, 'edited'):
                pipeline.write_guarded(path, b'regenerated', digest)
            self.assertEqual(path.read_bytes(), b'manual edit')
if __name__ == '__main__':
    unittest.main()

class LegacyCatalogTests(unittest.TestCase):
    def test_legacy_asset_ids_resolve_by_kind_index(self):
        catalogs = pipeline.catalog_inventory(ROOT)
        source = {'tileset': 'Graveyard', 'reference_counts': {
            pipeline.ref_key(['PROP', 'prop_1', 'Wastelands']): 1,
            pipeline.ref_key(['DECOR', 'decor_13', 'Wastelands']): 1}}
        self.assertEqual(pipeline.baseline_issues(source, catalogs), [])

    def test_no_silent_art_dropping_or_foreign_sheet_mapping(self):
        key = pipeline.ref_key(['PROP', 'prop_1', 'Forest'])
        source = {'reference_counts': {key: 1}}
        profile = {'target': 'Caverns', 'permitted_sheets': ['Caverns'],
                   'mappings': {key: {'visual_review': 'review-1', 'geometry_review': 'review-2',
                                      'replacements': []}}}
        with self.assertRaisesRegex(ValueError, 'drop art'):
            pipeline.validate_profile(source, profile)
        profile['mappings'][key]['replacements'] = [{'sheet': 'Forest'}]
        with self.assertRaisesRegex(ValueError, 'foreign sheet'):
            pipeline.validate_profile(source, profile)
        profile['mappings'][key]['replacements'] = [{'sheet': 'Caverns'}]
        profile['mappings'][key]['visual_review'] = ''
        with self.assertRaisesRegex(ValueError, 'unreviewed'):
            pipeline.validate_profile(source, profile)

class CandidateTests(unittest.TestCase):
    def test_three_exit_fence_room_mapping(self):
        import proof
        source = ROOT / 'TestGame/Rooms/Forest/forest/nwe.mroom'
        data, profile = proof.convert(source)
        self.assertEqual(proof.gameplay_lines(source.read_text()), proof.gameplay_lines(data))
        self.assertEqual(len(profile['mappings']), 20)
        self.assertNotIn('"Forest"', data)

    def test_west_room_mapping_preserves_full_layout(self):
        import proof
        source = ROOT / 'TestGame/Rooms/Forest/forest/w.mroom'
        data, profile = proof.convert(source)
        self.assertEqual(proof.gameplay_lines(source.read_text()), proof.gameplay_lines(data))
        self.assertNotIn('"Forest"', data)
        self.assertEqual(len(profile['mappings']), 14)

    def test_proof_candidate_preserves_gameplay_fields(self):
        import proof
        source = ROOT / 'TestGame/Rooms/Forest/forest/s.mroom'
        data, profile = proof.convert(source)
        self.assertIn('BIOME 6', data)
        self.assertIn('TILESET "Jungle"', data)
        self.assertNotIn('"Forest"', data)
        self.assertEqual(proof.gameplay_lines(source.read_text()), proof.gameplay_lines(data))
        self.assertEqual(proof.convert(source), (data, profile))
        self.assertEqual(len(profile['mappings']), 12)
        self.assertEqual(data.count('\nGROUND '), 448)
        self.assertEqual(data.count('\nVISUAL '), 84)
    def test_proof_rejects_unexpected_source(self):
        import proof
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'changed.mroom'
            path.write_text((ROOT / 'TestGame/Rooms/Forest/forest/s.mroom').read_text() + '\n')
            with self.assertRaisesRegex(ValueError, 'source hash'):
                proof.convert(path)
