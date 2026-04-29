(() => {
  function apply() {
    document.documentElement.lang = "en";
    import("/nav.js?v=1");
    if (location.pathname == "/help") import("/i18n-help.js?v=17");
  }

  if (document.readyState == "loading") {
    document.addEventListener("DOMContentLoaded", apply);
  } else {
    apply();
  }
})();
