<script setup>
import { toRefs } from "vue";
const props = defineProps(["valve"]);
const { valve } = toRefs(props);

const get_badge_class = () => {
  switch (valve.value.state) {
    case "open":
      return "bg-danger";
    case "closed":
      return "bg-success";
    default:
      return "bg-warning";
  }
};
</script>

<template>
  <div class="col-12 col-md-6 col-lg-3">
    <div class="card shadow text-center mb-3">
      <div class="card-body">
        <h1 class="card-title">{{ valve.name }}</h1>
        <h1 class="card-text mt-4">
          <span class="badge" :class="get_badge_class()">{{
            valve.state
          }}</span>
        </h1>
      </div>
    </div>
  </div>
</template>
