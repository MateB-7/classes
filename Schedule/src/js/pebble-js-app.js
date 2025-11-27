Pebble.addEventListener('showConfiguration', function() {
  var url = 'https://YOUR-GITHUB-USERNAME.github.io/pebble-schedule-config/config.html';
  Pebble.openURL(url);
});

Pebble.addEventListener('webviewclosed', function(e) {
  console.log('Configuration closed');
});
```

**Your project structure should look like:**
```
classes/
├── appinfo.json
├── package.json
├── wscript
└── src/
    ├── main.c
    └── js/
        └── pebble-js-app.js