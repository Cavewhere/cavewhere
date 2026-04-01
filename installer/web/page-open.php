<?php
/**
 * Template Name: CaveWhere Open
 * Description: Handles cavewhere:// deep link handoff — standalone, no theme chrome
 */

// Discard anything WordPress/BoldGrid buffered before this template ran.
while (ob_get_level() > 0) {
    ob_end_clean();
}
header('Content-Type: text/html; charset=utf-8');
header('X-Frame-Options: DENY');
header('Content-Security-Policy: default-src \'none\'; style-src \'self\' \'unsafe-inline\' https://fonts.googleapis.com; font-src https://fonts.gstatic.com; img-src https://cavewhere.com; script-src \'unsafe-inline\'');
?><!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Opening CaveWhere…</title>
  <link rel="preconnect" href="https://fonts.googleapis.com">
  <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
  <link href="https://fonts.googleapis.com/css2?family=Yanone+Kaffeesatz:wght@300;400&family=Fira+Mono&display=swap" rel="stylesheet">
  <style>
    *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }

    body {
      background: #111b27;
      color: #c8d8e8;
      font-family: "Fira Mono", "Lucida Console", Monaco, monospace;
      font-size: 14px;
      min-height: 100vh;
      display: flex;
      align-items: center;
      justify-content: center;
    }

    .card {
      text-align: center;
      padding: 3rem 2rem;
      max-width: 420px;
      width: 100%;
    }

    .logo {
      width: 220px;
      margin-bottom: 2.5rem;
      opacity: 0.92;
    }

    .status {
      font-family: "Yanone Kaffeesatz", Helvetica, Arial, sans-serif;
      font-size: 28px;
      letter-spacing: 3px;
      text-transform: uppercase;
      color: #4f6f96;
      margin-bottom: 1.2rem;
    }

    /* Three-dot pulse */
    .dots span {
      display: inline-block;
      width: 7px;
      height: 7px;
      margin: 0 3px;
      border-radius: 50%;
      background: #4f6f96;
      animation: pulse 1.2s ease-in-out infinite;
    }
    .dots span:nth-child(2) { animation-delay: 0.2s; }
    .dots span:nth-child(3) { animation-delay: 0.4s; }

    @keyframes pulse {
      0%, 80%, 100% { opacity: 0.2; transform: scale(0.85); }
      40%            { opacity: 1;   transform: scale(1);    }
    }

    .fallback {
      display: none;
      margin-top: 2rem;
      line-height: 1.7;
      color: #8a9aaa;
    }

    .fallback p { margin-bottom: 1rem; }

    .download-link {
      display: inline-block;
      font-family: "Yanone Kaffeesatz", Helvetica, Arial, sans-serif;
      font-size: 16px;
      letter-spacing: 2px;
      text-transform: uppercase;
      color: #4f6f96;
      border: 1px solid #4f6f96;
      padding: 0.55rem 1.4rem;
      text-decoration: none;
      border-radius: 2px;
      transition: background 0.2s, color 0.2s;
    }
    .download-link:hover {
      background: #4f6f96;
      color: #111b27;
    }

    .no-repo {
      font-family: "Yanone Kaffeesatz", Helvetica, Arial, sans-serif;
      font-size: 22px;
      letter-spacing: 2px;
      text-transform: uppercase;
      color: #e07070;
      margin-bottom: 0.8rem;
    }
  </style>
</head>
<body>
  <div class="card">
    <img class="logo"
         src="https://cavewhere.com/wp-content/uploads/2020/01/cavewhere_stacked_256px-withShadow-01.png"
         alt="CaveWhere"
         onerror="this.style.display='none'">

    <div id="state-opening">
      <div class="status" id="status-text">Opening CaveWhere</div>
      <div class="dots" id="dots"><span></span><span></span><span></span></div>
    </div>

    <div id="state-no-repo" style="display:none">
      <div class="no-repo">Invalid link</div>
      <p id="no-repo-message" style="color:#8a9aaa">No repository was specified in this link.</p>
    </div>

    <div class="fallback" id="fallback">
      <p>CaveWhere doesn&rsquo;t appear to be installed.</p>
      <a class="download-link" href="https://cavewhere.com/download">Download CaveWhere</a>
    </div>
  </div>

  <script>
    (function () {
      var params = new URLSearchParams(window.location.search);
      var repo   = params.get("repo");

      function isValidRepo(url) {
        try {
          var u = new URL(url);
          return u.protocol === "https:" &&
                 /^(github\.com|gitlab\.com|bitbucket\.org)$/.test(u.hostname);
        } catch (e) { return false; }
      }

      if (!repo || repo.length > 1024 || !isValidRepo(repo)) {
        document.getElementById("state-opening").style.display = "none";
        document.getElementById("state-no-repo").style.display = "block";
        document.getElementById("no-repo-message").textContent = repo
          ? "The link does not point to a supported repository host (github.com, gitlab.com, or bitbucket.org)."
          : "No repository was specified in this link.";
        document.title = "Invalid Link \u2014 CaveWhere";
        return;
      }

      // Redirect to the custom scheme — the OS hands it to the app if installed.
      window.location = "cavewhere://open?repo=" + encodeURIComponent(repo);

      // After 3 s with no handoff, assume the app isn't installed.
      var fallbackTimer = setTimeout(function () {
        document.getElementById("fallback").style.display = "block";
      }, 3000);

      var appOpened = false;

      function onHandoff() {
        if (appOpened) return;
        appOpened = true;
        clearTimeout(fallbackTimer);
      }

      function onReturn() {
        if (!appOpened) return;
        document.getElementById("status-text").textContent = "Map Cave";
        document.getElementById("dots").style.display = "none";
      }

      document.addEventListener("visibilitychange", function () {
        if (document.hidden) onHandoff(); else onReturn();
      });
      window.addEventListener("blur", onHandoff);
      window.addEventListener("focus", onReturn);
    })();
  </script>
</body>
</html>
<?php exit; ?>
