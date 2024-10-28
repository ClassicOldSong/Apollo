<script setup>
import { ref, onMounted } from 'vue'

const props = defineProps({
  platform: String,
  config: Object,
  globalPrepCmd: Array,
  serverCmd: Array
})

const config = ref(props.config)
const globalPrepCmd = ref(props.globalPrepCmd)
const serverCmd = ref(props.serverCmd)

const prepCmdTemplate = {
  do: "",
  undo: "",
}

const serverCmdTemplate = {
  name: "",
  cmd: ""
}

function addCmd(cmdArr, template) {
  const _tpl = Object.assign({}, template);

  if (props.platform === 'windows') {
    _tpl.elevated = false;
  }
  cmdArr.push(_tpl);
}

function removeCmd(cmdArr, index) {
  cmdArr.splice(index,1)
}

onMounted(() => {
  // Set default value for enable_pairing if not present
  if (config.value.enable_pairing === undefined) {
    config.value.enable_pairing = "enabled"
  }
})
</script>

<template>
  <div id="general" class="config-page">
    <!-- Locale -->
    <div class="mb-3">
      <label for="locale" class="form-label">{{ $t('config.locale') }}</label>
      <select id="locale" class="form-select" v-model="config.locale">
        <option value="de">Deutsch (German)</option>
        <option value="en">English</option>
        <option value="en_GB">English, UK</option>
        <option value="en_US">English, US</option>
        <option value="es">Español (Spanish)</option>
        <option value="fr">Français (French)</option>
        <option value="it">Italiano (Italian)</option>
        <option value="ja">日本語 (Japanese)</option>
        <option value="pt">Português (Portuguese)</option>
        <option value="ru">Русский (Russian)</option>
        <option value="sv">svenska (Swedish)</option>
        <option value="tr">Türkçe (Turkish)</option>
        <option value="zh">简体中文 (Chinese Simplified)</option>
      </select>
      <div class="form-text">{{ $t('config.locale_desc') }}</div>
    </div>

    <!-- Apollo Name -->
    <div class="mb-3">
      <label for="sunshine_name" class="form-label">{{ $t('config.sunshine_name') }}</label>
      <input type="text" class="form-control" id="sunshine_name" placeholder="Apollo"
             v-model="config.sunshine_name" />
      <div class="form-text">{{ $t('config.sunshine_name_desc') }}</div>
    </div>

    <!-- Log Level -->
    <div class="mb-3">
      <label for="min_log_level" class="form-label">{{ $t('config.log_level') }}</label>
      <select id="min_log_level" class="form-select" v-model="config.min_log_level">
        <option value="0">{{ $t('config.log_level_0') }}</option>
        <option value="1">{{ $t('config.log_level_1') }}</option>
        <option value="2">{{ $t('config.log_level_2') }}</option>
        <option value="3">{{ $t('config.log_level_3') }}</option>
        <option value="4">{{ $t('config.log_level_4') }}</option>
        <option value="5">{{ $t('config.log_level_5') }}</option>
        <option value="6">{{ $t('config.log_level_6') }}</option>
      </select>
      <div class="form-text">{{ $t('config.log_level_desc') }}</div>
    </div>

    <!-- Global Prep Commands -->
    <div id="global_prep_cmd" class="mb-3 d-flex flex-column">
      <label class="form-label">{{ $t('config.global_prep_cmd') }}</label>
      <div class="form-text">{{ $t('config.global_prep_cmd_desc') }}</div>
      <table class="table" v-if="globalPrepCmd.length > 0">
        <thead>
        <tr>
          <th scope="col"><i class="fas fa-play"></i> {{ $t('_common.do_cmd') }}</th>
          <th scope="col"><i class="fas fa-undo"></i> {{ $t('_common.undo_cmd') }}</th>
          <th scope="col" v-if="platform === 'windows'">
            <i class="fas fa-shield-alt"></i> {{ $t('_common.run_as') }}
          </th>
          <th scope="col"></th>
        </tr>
        </thead>
        <tbody>
        <tr v-for="(c, i) in globalPrepCmd">
          <td>
            <input type="text" class="form-control monospace" v-model="c.do" />
          </td>
          <td>
            <input type="text" class="form-control monospace" v-model="c.undo" />
          </td>
          <td v-if="platform === 'windows'">
            <div class="form-check">
              <input type="checkbox" class="form-check-input" :id="'prep-cmd-admin-' + i" v-model="c.elevated"
                     true-value="true" false-value="false" />
              <label :for="'prep-cmd-admin-' + i" class="form-check-label">{{ $t('_common.elevated') }}</label>
            </div>
          </td>
          <td>
            <button class="btn btn-danger me-2" @click="removeCmd(globalPrepCmd, i)">
              <i class="fas fa-trash"></i>
            </button>
            <button class="btn btn-success" @click="addCmd(globalPrepCmd, prepCmdTemplate)">
              <i class="fas fa-plus"></i>
            </button>
          </td>
        </tr>
        </tbody>
      </table>
      <button class="ms-0 mt-2 btn btn-success" style="margin: 0 auto" @click="addCmd(globalPrepCmd, prepCmdTemplate)">
        &plus; {{ $t('config.add') }}
      </button>
    </div>

    <!-- Server Commands -->
    <div id="server_cmd" class="mb-3 d-flex flex-column">
      <label class="form-label">{{ $t('config.server_cmd') }}</label>
      <div class="form-text">{{ $t('config.server_cmd_desc') }}</div>
      <div class="form-text">
        <a href="https://github.com/ClassicOldSong/Apollo/wiki/Server-Commands" target="_blank">{{ $t('_common.learn_more') }}</a>
      </div>
      <table class="table" v-if="serverCmd.length > 0">
        <thead>
        <tr>
          <th scope="col"><i class="fas fa-tag"></i> {{ $t('_common.cmd_name') }}</th>
          <th scope="col"><i class="fas fa-terminal"></i> {{ $t('_common.cmd_val') }}</th>
          <th scope="col" v-if="platform === 'windows'">
            <i class="fas fa-shield-alt"></i> {{ $t('_common.run_as') }}
          </th>
          <th scope="col"></th>
        </tr>
        </thead>
        <tbody>
        <tr v-for="(c, i) in serverCmd">
          <td>
            <input type="text" class="form-control" v-model="c.name" />
          </td>
          <td>
            <input type="text" class="form-control monospace" v-model="c.cmd" />
          </td>
          <td v-if="platform === 'windows'">
            <div class="form-check">
              <input type="checkbox" class="form-check-input" :id="'server-cmd-admin-' + i" v-model="c.elevated"
                     true-value="true" false-value="false" />
              <label :for="'server-cmd-admin-' + i" class="form-check-label">{{ $t('_common.elevated') }}</label>
            </div>
          </td>
          <td>
            <button class="btn btn-danger me-2" @click="removeCmd(serverCmd, i)">
              <i class="fas fa-trash"></i>
            </button>
            <button class="btn btn-success" @click="addCmd(serverCmd, serverCmdTemplate)">
              <i class="fas fa-plus"></i>
            </button>
          </td>
        </tr>
        </tbody>
      </table>
      <button class="ms-0 mt-2 btn btn-success" style="margin: 0 auto" @click="addCmd(serverCmd, serverCmdTemplate)">
        &plus; {{ $t('config.add') }}
      </button>
    </div>

    <!-- Enable Pairing -->
    <div class="mb-3 form-check">
      <input type="checkbox" class="form-check-input" id="enable_pairing" v-model="config.enable_pairing" true-value="enabled" false-value="disabled"/>
      <label for="enable_pairing" class="form-check-label">{{ $t('config.enable_pairing') }}</label>
      <div class="form-text">{{ $t('config.enable_pairing_desc') }}</div>
    </div>

    <!-- Hide Tray Controls -->
    <div class="mb-3 form-check">
      <input type="checkbox" class="form-check-input" id="hide_tray_controls" v-model="config.hide_tray_controls" true-value="enabled" false-value="disabled"/>
      <label for="hide_tray_controls" class="form-check-label">{{ $t('config.hide_tray_controls') }}</label>
      <div class="form-text">{{ $t('config.hide_tray_controls_desc') }}</div>
    </div>

    <!-- Notify Pre-Releases -->
    <div class="mb-3 form-check">
      <input type="checkbox" class="form-check-input" id="notify_pre_releases" v-model="config.notify_pre_releases" true-value="enabled" false-value="disabled"/>
      <label for="notify_pre_releases" class="form-check-label">{{ $t('config.notify_pre_releases') }}</label>
      <div class="form-text">{{ $t('config.notify_pre_releases_desc') }}</div>
    </div>
  </div>
</template>

<style scoped>

</style>
