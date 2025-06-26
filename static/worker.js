// service-worker.js

self.addEventListener("push", (event) => {
  console.log("[Service Worker] Push Recebido.");

  // O backend envia os dados como texto, então precisamos parsear para JSON.
  const data = event.data.json();
  console.log("[Service Worker] Dados da notificação:", data);

  const title = data.title || "Notificação";
  const options = {
    body: data.body || "Algo aconteceu!",
    icon: data.icon, // O ícone que definimos no backend
    badge: "badge.png", // Um ícone menor para a barra de status do Android
  };

  // O Service Worker precisa se manter vivo até a notificação ser mostrada.
  event.waitUntil(self.registration.showNotification(title, options));
});

// Opcional: Lidar com o clique na notificação
// self.addEventListener("notificationclick", (event) => {
//   console.log("[Service Worker] Notificação clicada.");
//   event.notification.close();

//   // Abre a página do seu site quando a notificação é clicada
//   event.waitUntil(
//     clients.openWindow("http://localhost:8080") // Mude se necessário
//   );
// });
