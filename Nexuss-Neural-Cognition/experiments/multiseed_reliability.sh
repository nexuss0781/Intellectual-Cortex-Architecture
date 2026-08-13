#!/usr/bin/env bash
set -euo pipefail

exe="$1"
out="$2"
mkdir -p "$out"
seeds=(11 22 33 44 55)
summary="$out/summary.txt"
printf 'B5_MULTI_SEED_RELIABILITY=PASS\n' > "$summary"
printf 'seed,run,accuracy,restart_accuracy,replay_events,restart_match,weight_hash\n' > "$out/results.csv"
pass=1
sum=0
count=0
for seed in "${seeds[@]}"; do
  for run in 1 2; do
    dir="$out/seed-${seed}-run-${run}"
    rm -rf "$dir"
    mkdir -p "$dir"
    "$exe" "$seed" "$dir" > "$dir/stdout.txt"
    grep -q 'B3_CONTEXTUAL_DECISION=PASS' "$dir/stdout.txt" || pass=0
    accuracy=$(awk -F= '/^contextual_accuracy=/{print $2}' "$dir/summary.txt")
    restart=$(awk -F= '/^restart_accuracy=/{print $2}' "$dir/summary.txt")
    replay=$(awk -F= '/^replay_events=/{print $2}' "$dir/summary.txt")
    restart_match=$(awk -F= '/^restart_match=/{print $2}' "$dir/summary.txt")
    hash=$(sha256sum "$dir/contextual/weights.state" | awk '{print $1}')
    printf '%s,%s,%s,%s,%s,%s,%s\n' "$seed" "$run" "$accuracy" "$restart" "$replay" "$restart_match" "$hash" >> "$out/results.csv"
    awk "BEGIN { if ($accuracy < 0.80 || $restart < 0.80 || $replay <= 0 || $restart_match != 1) exit 1 }" || pass=0
    sum=$(awk "BEGIN {print $sum + $accuracy}")
    count=$((count + 1))
  done
  h1=$(awk -F, -v s="$seed" '$1==s && $2==1 {print $7}' "$out/results.csv")
  h2=$(awk -F, -v s="$seed" '$1==s && $2==2 {print $7}' "$out/results.csv")
  [[ "$h1" == "$h2" ]] || pass=0
  printf 'seed_%s_hash_match=%s\n' "$seed" "$([[ "$h1" == "$h2" ]] && echo 1 || echo 0)" >> "$summary"
done
mean=$(awk "BEGIN {printf \"%.6f\", $sum / $count}")
printf 'runs=%s\nmean_accuracy=%s\n' "$count" "$mean" >> "$summary"
awk "BEGIN { if ($mean < 0.90) exit 1 }" || pass=0
if [[ "$pass" -eq 1 ]]; then sed -i '1s/FAIL/PASS/' "$summary"; else sed -i '1s/PASS/FAIL/' "$summary"; fi
cat "$summary"
[[ "$pass" -eq 1 ]]
