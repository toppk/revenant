/* Tables work without JavaScript. Filtering progressively enhances static HTML. */
function initializeCompatibility() {
  document.querySelectorAll('.tdn-comparison').forEach((view) => {
    if (view.dataset.initialized) return;
    view.dataset.initialized = 'true';
    const controls = view.querySelector('.tdn-controls');
    const search = view.querySelector('[data-tdn-search]');
    const policy = view.querySelector('[data-tdn-label]');
    const differences = view.querySelector('[data-tdn-differences]');
    const choices = Array.from(view.querySelectorAll('[data-tdn-terminal]'));
    const rows = Array.from(view.querySelectorAll('tr[data-feature]'));
    function update() {
      const selected = new Set(choices.filter((c) => c.checked).map((c) => c.dataset.tdnTerminal));
      const query = search.value.trim().toLowerCase();
      let visible = 0;
      view.querySelectorAll('[data-terminal]').forEach((cell) => {
        cell.hidden = !selected.has(cell.dataset.terminal);
      });
      rows.forEach((row) => {
        const statuses = Array.from(row.querySelectorAll('td[data-terminal]'))
          .filter((cell) => selected.has(cell.dataset.terminal))
          .map((cell) => cell.dataset.support);
        row.hidden = !row.dataset.search.includes(query)
          || (policy.value && !row.dataset.labels.split(' ').includes(policy.value))
          || (differences.checked && new Set(statuses).size < 2);
        if (!row.hidden) visible++;
      });
      view.querySelector('[data-tdn-count]').textContent = `${visible} of ${rows.length} features · ${selected.size} terminals selected`;
    }
    controls.hidden = false;
    controls.addEventListener('input', update);
    controls.addEventListener('change', update);
    view.querySelector('[data-tdn-all]').addEventListener('click', () => {
      choices.forEach((choice) => { choice.checked = true; });
      update();
    });
    update();
  });
}
if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', initializeCompatibility);
} else {
  initializeCompatibility();
}
// Also support Material's optional instant navigation if enabled later.
if (typeof document$ !== 'undefined') document$.subscribe(initializeCompatibility);
