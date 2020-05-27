import json

SPEED_OUTPUT_PATH = "./output/speed_meta.json"
FACE_OUTPUT_PATH = "./output/face_meta.json"
HAND_OUTPUT_PATH = "./output/hand_meta.json"


class MetaCollector:
    def __init__(self):
        self._frame_number = 0
        self._data = []
        self._hands = {'left_hand': [], 'right_hand': []}

    def process_frame(self, frame):
        self._frame_number += 1
        for region in frame.regions():
            for tensor in region.tensors():
                if self._is_speed_detection(tensor):
                    self._update_speed_data(tensor)
                elif self._is_face_detection(tensor):
                    self._update_face_data(tensor)
                elif self._is_hand_detection(tensor):
                    self._update_hand_data(tensor, region)
        # print(self._frame_number)

    def _is_speed_detection(self, tensor):
        return tensor.has_field('velocity')

    def _is_face_detection(self, tensor):
        return tensor.has_field('attribute_name') \
               and tensor.has_field('format') \
               and tensor.has_field('label') \
               and tensor.has_field('label_id') \
               and tensor.has_field('cos_dist') \
               and tensor['attribute_name'] == 'face_id' \
               and tensor['format'] == 'cosine_distance'

    def _is_hand_detection(self, tensor):
        return tensor.has_field('attribute_name') \
               and tensor.has_field('label') \
               and tensor['attribute_name'] == 'hand_type'

    def _update_speed_data(self, tensor):
        speed = tensor['velocity']
        self._data.append(speed)
        self._write_json(SPEED_OUTPUT_PATH)

    def _update_face_data(self, tensor):
        face = {
            "label": tensor['label'],
            "id": tensor['label_id'],
            "cosine_distance": tensor['cos_dist']
        }
        self._data.append(face)
        self._write_json(FACE_OUTPUT_PATH)

    def _update_hand_data(self, tensor, region):
        hand = tensor['label']
        self._hands[region.label()].append(hand)
        self._write_json(HAND_OUTPUT_PATH, struct='hands')

    def _write_json(self, filename, struct='data'):
        data = self._data if struct=='data' else self._hands
        with open(filename, 'w') as f:
            json.dump(data, f, indent=4, sort_keys=True)
