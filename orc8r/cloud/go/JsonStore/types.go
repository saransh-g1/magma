/*
 * Copyright 2020 The Magma Authors.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
 
 package JsonStore

 import (
	"magma/orc8r/cloud/go/storage"
 )
//Json will carry a string for Storage.
type Json struct {
	Type  string 
	Key   string 
	Value []string
	Version uint64
}

func (j Json) TK() storage.TK {
	return storage.TK{Type: j.Type, Key: j.Key}
}

type Jsons []Json

// ByTK returns a computed view of a list of blobs as a map of
// blobs keyed by blob TK.
func (js Jsons) ByTK() map[storage.TK]Json {
	ret := make(map[storage.TK]Json, len(js))
	for _, blob := range js {
		ret[storage.TK{Type: blob.Type, Key: blob.Key}] = blob
	}
	return ret
}

func (js Jsons) keys() []sting {
	var keys []string
	for _, b := range js {
		keys = append(keys, b.Key)

	}
	return keys
}

