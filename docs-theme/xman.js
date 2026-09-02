/* xman-style navigation: menus, directory, both screens, search, reverse video. */
(function () {
  "use strict";

  var XMAN = window.XMAN || {};
  var WHATIS = JSON.parse(document.getElementById("whatis").textContent);
  var el = function (id) { return document.getElementById(id); };
  var dirpane = el("dirpane"), manpane = el("manpane"), grip = el("grip"), panes = el("panes");
  var man = el("man"), manscroll = el("manscroll"), sb = el("sb"), th = el("th"), label = el("label");
  var pageHtml = man.innerHTML, pageLabel = label.innerHTML;
  var state = { dir: false, both: false, manual: XMAN.manual };

  function store(key, value) { try { localStorage.setItem(key, value); } catch (e) {} }
  function load(key) { try { return localStorage.getItem(key); } catch (e) { return null; } }
  function href(entry) { return XMAN.base.replace(/\/?$/, "/") + entry.url; }

  /* ---- layout ---- */
  function layout() {
    panes.classList.toggle("both", state.both);
    if (state.both) { dirpane.hidden = false; manpane.hidden = false; grip.hidden = false; }
    else { dirpane.hidden = !state.dir; manpane.hidden = state.dir; grip.hidden = true; }
    updateThumb();
  }
  function showDirectory(manual) {
    if (manual) state.manual = manual;
    Array.prototype.forEach.call(document.querySelectorAll(".directory"), function (d) {
      d.hidden = d.dataset.manual !== state.manual;
    });
    state.dir = true;
    layout();
  }
  function showPage() {
    man.innerHTML = pageHtml; label.innerHTML = pageLabel;
    state.dir = false; layout();
  }
  function toggleBoth() {
    state.both = !state.both;
    el("bothitem").setAttribute("aria-checked", state.both ? "true" : "false");
    store("xman-both", state.both ? "1" : "0");
    if (state.both) showDirectory(); else layout();
  }
  function toggleRv() {
    var on = document.documentElement.classList.toggle("rv");
    el("rvitem").setAttribute("aria-checked", on ? "true" : "false");
    store("xman-rv", on ? "1" : "0");
  }
  function showHelp() {
    man.innerHTML = el("helptext").innerHTML;
    label.textContent = "Xman Help";
    state.dir = false; layout(); manscroll.scrollTop = 0;
  }

  /* ---- Athena scrollbar ---- */
  function updateThumb() {
    if (manpane.hidden) return;
    var h = manscroll.clientHeight, sh = manscroll.scrollHeight || 1;
    th.style.height = Math.max(6, Math.min(1, h / sh) * 100) + "%";
    th.style.top = (manscroll.scrollTop / sh * 100) + "%";
  }
  manscroll.addEventListener("scroll", updateThumb);
  window.addEventListener("resize", updateThumb);
  sb.addEventListener("mousedown", function (e) {
    var r = sb.getBoundingClientRect(), y = e.clientY - r.top;
    if (e.button === 2) manscroll.scrollTop -= y;
    else if (e.button === 1) manscroll.scrollTop = (y / r.height) * manscroll.scrollHeight;
    else manscroll.scrollTop += y;
    e.preventDefault();
  });
  sb.addEventListener("contextmenu", function (e) { e.preventDefault(); });

  /* ---- menus: hover opens, click toggles, keyboard walks ---- */
  var hoverTimer = null;
  function closeMenus() {
    ["optbtn", "secbtn"].forEach(function (id) { el(id).setAttribute("aria-expanded", "false"); });
    ["optmenu", "secmenu"].forEach(function (id) { el(id).hidden = true; });
  }
  function openMenu(btnId, menuId, focus) {
    closeMenus();
    el(btnId).setAttribute("aria-expanded", "true");
    el(menuId).hidden = false;
    if (focus) { var first = el(menuId).querySelector(".item"); if (first) first.focus(); }
  }
  [["optbtn", "optmenu"], ["secbtn", "secmenu"]].forEach(function (pair) {
    var b = el(pair[0]);
    b.addEventListener("mouseenter", function () {
      clearTimeout(hoverTimer);
      if (b.getAttribute("aria-expanded") !== "true") openMenu(pair[0], pair[1], false);
    });
    b.addEventListener("mouseleave", function () { clearTimeout(hoverTimer); hoverTimer = setTimeout(closeMenus, 220); });
    b.addEventListener("click", function (e) {
      if (e.target.closest(".item")) return;
      clearTimeout(hoverTimer);
      if (b.getAttribute("aria-expanded") === "true") closeMenus(); else openMenu(pair[0], pair[1], true);
      e.stopPropagation();
    });
    el(pair[1]).addEventListener("keydown", function (e) {
      var items = Array.prototype.slice.call(this.querySelectorAll(".item")), i = items.indexOf(document.activeElement);
      if (e.key === "ArrowDown") { items[(i + 1) % items.length].focus(); e.preventDefault(); }
      else if (e.key === "ArrowUp") { items[(i - 1 + items.length) % items.length].focus(); e.preventDefault(); }
      else if (e.key === "Enter" || e.key === " ") { document.activeElement.click(); e.preventDefault(); }
      else if (e.key === "Escape") { closeMenus(); b.focus(); e.preventDefault(); }
    });
  });
  document.addEventListener("click", function (e) { if (!e.target.closest(".menubtn")) closeMenus(); });

  function act(name) {
    closeMenus();
    switch (name) {
      case "dir": showDirectory(); break;
      case "page": showPage(); break;
      case "help": showHelp(); break;
      case "search": openSearch(); break;
      case "both": toggleBoth(); break;
      case "rv": toggleRv(); break;
    }
  }
  el("optmenu").addEventListener("click", function (e) {
    var it = e.target.closest(".item"); if (!it || it.tagName === "A") return;
    act(it.dataset.act); e.stopPropagation();
  });
  el("secmenu").addEventListener("click", function (e) {
    var it = e.target.closest(".item"); if (!it || it.tagName === "A") return;
    closeMenus(); showDirectory(it.dataset.manual); e.stopPropagation();
  });

  /* ---- search: one box, live results over the page index ---- */
  var shell = el("searchshell"), input = el("searchin"), results = el("results");
  var index = null, indexPromise = null, selected = 0, matches = [];
  var byUrl = {};
  WHATIS.forEach(function (e) { byUrl[e.url] = e; });

  function loadIndex() {
    if (indexPromise) return indexPromise;
    indexPromise = fetch(XMAN.base.replace(/\/?$/, "/") + "search/search_index.json")
      .then(function (r) { return r.json(); })
      .then(function (data) {
        index = data.docs.map(function (d) {
          var url = d.location.split("#")[0], hash = d.location.indexOf("#") >= 0 ? d.location.slice(d.location.indexOf("#")) : "";
          var entry = byUrl[url] || byUrl[url + "/"] || null;
          return { url: url, hash: hash, entry: entry, title: d.title || "", text: d.text || "",
                   haystack: ((entry ? entry.man + " " + entry.description + " " : "") + d.title + " " + d.text).toLowerCase() };
        });
        return index;
      })
      .catch(function () { index = []; return index; });
    return indexPromise;
  }
  function openSearch() {
    shell.hidden = false; input.value = ""; results.innerHTML = ""; matches = [];
    loadIndex();
    setTimeout(function () { input.focus(); }, 0);
  }
  function closeSearch() { shell.hidden = true; }
  function escapeHtml(s) { return s.replace(/[&<>"]/g, function (c) { return { "&": "&amp;", "<": "&lt;", ">": "&gt;", "\"": "&quot;" }[c]; }); }
  function snippet(text, tokens) {
    var lower = text.toLowerCase(), at = -1;
    for (var i = 0; i < tokens.length && at < 0; i++) at = lower.indexOf(tokens[i]);
    if (at < 0) return text.slice(0, 120);
    var start = Math.max(0, at - 50), end = Math.min(text.length, at + 90);
    return (start ? "\u2026" : "") + text.slice(start, end) + (end < text.length ? "\u2026" : "");
  }
  function search(q) {
    var tokens = q.toLowerCase().split(/\s+/).filter(Boolean);
    if (!index || !tokens.length) { matches = []; render(); return; }
    var seen = {};
    matches = index.filter(function (d) {
      return tokens.every(function (t) { return d.haystack.indexOf(t) >= 0; });
    }).map(function (d) {
      var score = 0, name = d.entry ? d.entry.man : "", title = d.title.toLowerCase();
      tokens.forEach(function (t) {
        if (name === t) score += 100; else if (name.indexOf(t) >= 0) score += 40;
        if (title.indexOf(t) >= 0) score += 20;
        if (!d.hash) score += 5;
      });
      return { d: d, score: score };
    }).sort(function (x, y) { return y.score - x.score; })
      .filter(function (m) { var k = m.d.url + m.d.hash; if (seen[k]) return false; seen[k] = true; return true; })
      .slice(0, 12).map(function (m) { return m.d; });
    selected = 0; render(tokens);
  }
  function render(tokens) {
    results.innerHTML = matches.map(function (d, i) {
      var name = d.entry ? d.entry.man + "(" + d.entry.section + ")" : d.title;
      var where = d.hash ? d.title : (d.entry ? d.entry.description : "");
      return '<a role="option" class="result' + (i === selected ? " sel" : "") + '" href="' + href({ url: d.url }) + d.hash + '">' +
        '<span class="rname">' + escapeHtml(name) + '</span><span class="rwhere">' + escapeHtml(where) + '</span>' +
        '<span class="rsnip">' + escapeHtml(snippet(d.text, tokens || [])) + '</span></a>';
    }).join("");
  }
  function openSelected() {
    if (!matches.length) return;
    var d = matches[Math.min(selected, matches.length - 1)];
    location.href = href({ url: d.url }) + d.hash;
  }
  input.addEventListener("input", function () {
    var q = input.value.trim();
    loadIndex().then(function () { if (input.value.trim() === q) search(q); });
  });
  input.addEventListener("keydown", function (e) {
    if (e.key === "Enter") { openSelected(); e.preventDefault(); }
    else if (e.key === "ArrowDown") { selected = Math.min(selected + 1, matches.length - 1); render(input.value.toLowerCase().split(/\s+/)); e.preventDefault(); }
    else if (e.key === "ArrowUp") { selected = Math.max(selected - 1, 0); render(input.value.toLowerCase().split(/\s+/)); e.preventDefault(); }
    else if (e.key === "Escape") closeSearch();
  });
  results.addEventListener("mousemove", function (e) {
    var a = e.target.closest(".result"); if (!a) return;
    var i = Array.prototype.indexOf.call(results.children, a);
    if (i !== selected) { selected = i; render(input.value.toLowerCase().split(/\s+/)); }
  });
  el("s-open").addEventListener("click", openSelected);
  el("s-cancel").addEventListener("click", closeSearch);
  shell.addEventListener("click", function (e) { if (e.target === shell) closeSearch(); });

  /* ---- accelerators ---- */
  document.addEventListener("keydown", function (e) {
    if (!(e.ctrlKey && !e.altKey && !e.metaKey && !e.shiftKey)) {
      if (e.key === "Escape") { closeMenus(); closeSearch(); }
      return;
    }
    if (e.target === input) return;
    var handled = true;
    switch (e.key.toLowerCase()) {
      case "d": showDirectory(); break;
      case "m": showPage(); break;
      case "b": toggleBoth(); break;
      case "s": openSearch(); break;
      case "h": showHelp(); break;
      case "r": toggleRv(); break;
      default: handled = false;
    }
    if (handled) { closeMenus(); e.preventDefault(); }
  });

  /* ---- startup ---- */
  if (document.documentElement.classList.contains("rv")) el("rvitem").setAttribute("aria-checked", "true");
  if (load("xman-both") === "1") { state.both = true; el("bothitem").setAttribute("aria-checked", "true"); showDirectory(); }
  else layout();
  if (location.hash) { var target = document.getElementById(location.hash.slice(1)); if (target) target.scrollIntoView(); }
})();
