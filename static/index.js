// A chave pública VAPID que seu backend usa.
const VAPID_PUBLIC_KEY =
  "BOBHannYUxzTH1mLsRLGM5_Kl90Bs2uCS6-v7ptabgrsh_Iu-9iyiKRsVGArdeciYdljjw3tqT7-fEG5Qdh6PSE";

function urlBase64ToUint8Array(base64String) {
  const padding = "=".repeat((4 - (base64String.length % 4)) % 4);
  const base64 = (base64String + padding).replace(/-/g, "+").replace(/_/g, "/");
  const rawData = window.atob(base64);
  const outputArray = new Uint8Array(rawData.length);
  for (let i = 0; i < rawData.length; ++i) {
    outputArray[i] = rawData.charCodeAt(i);
  }
  return outputArray;
}

// registra o service worker e pede a inscrição
async function subscribeUser() {
  // 1. Registra o Service Worker
  const swRegistration = await navigator.serviceWorker.register("worker.js");
  console.log("Service Worker registrado:", swRegistration);

  // 2. Pede a inscrição para o serviço de push
  try {
    const subscription = await swRegistration.pushManager.subscribe({
      userVisibleOnly: true, // Sempre true
      applicationServerKey: urlBase64ToUint8Array(VAPID_PUBLIC_KEY),
    });
    console.log("Inscrição de Push obtida:", subscription);

    // 3. Envia a inscrição para o backend
    await fetch("/api/subscribe", {
      method: "POST",
      body: JSON.stringify(subscription),
      headers: {
        "Content-Type": "application/json",
      },
    });
    console.log("Inscrição enviada para o servidor.");
    alert("Inscrição realizada com sucesso!");
  } catch (error) {
    console.error("Falha ao se inscrever:", error);
    if (Notification.permission === "denied") {
      alert(
        "Você bloqueou as notificações. Por favor, habilite nas configurações do site."
      );
    } else {
      alert("Não foi possível se inscrever para notificações.");
    }
  }
}

const subscribeButton = document.getElementById("subscribeButton");
// const triggerButton = document.getElementById("triggerButton");

subscribeButton.addEventListener("click", () => {
  if ("serviceWorker" in navigator && "PushManager" in window) {
    subscribeUser();
  } else {
    alert("Seu navegador não suporta notificações push.");
  }
});

// triggerButton.addEventListener("click", () => {
//   fetch("/hello")
//     .then((res) => res.text())
//     .then((text) => console.log("Resposta do /hello:", text))
//     .catch((err) => console.error(err));
// });

setInterval(async () => {
  const response = await fetch("/api/time");
  const body = await response.text();

  document.getElementById("time").innerHTML = body;
}, 1000);
