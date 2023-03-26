<script setup>
import axios from "axios";
import { ref } from "vue";
import ValveCard from "./components/ValveCard.vue";

const valves = ref([]);

const update = () => {
  axios.get("/status").then((response) => {
    console.log(response.data);
    valves.value = response.data.valves;
  });
};

update();

setInterval(update, 3000);
</script>

<template>
  <div class="container">
    <div class="row">
      <h1 class="display-1 mb-4 mt-4">Valvola</h1>
    </div>

    <div v-if="valves" class="row">
      <ValveCard v-for="(valve, index) in valves" :key="index" :valve="valve" />
    </div>
    <div v-else class="row">
      <div class="col-12 text-center">
        <div class="spinner-border text-center" role="status"></div>
      </div>
    </div>
  </div>
</template>
