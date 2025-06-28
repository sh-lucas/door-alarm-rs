use base64::{engine::general_purpose::URL_SAFE_NO_PAD, Engine as _};
//use dotenvy::dotenv;
use poem::{
    endpoint::StaticFilesEndpoint,
    handler,
    http::StatusCode,
    listener::TcpListener,
    web::{Data, Json},
    Endpoint, EndpointExt, Result, Route, /*Server*/
};
use serde::{Deserialize, Serialize};
use std::sync::Arc;
use tokio::sync::Mutex;
use tokio::time::Instant;

use web_push::{
    ContentEncoding, IsahcWebPushClient, SubscriptionInfo, VapidSignatureBuilder, WebPushClient,
    WebPushMessageBuilder,
};

// --- Imports do Shuttle ---
use shuttle_poem::ShuttlePoem;
use shuttle_secrets::SecretStore;
// use shuttle_static_folder::StaticFolder;
use std::path::PathBuf;

// --- Structs ---

#[derive(Debug, Clone, Deserialize)]
pub struct PushSubscriptionKeys {
    p256dh: String,
    auth: String,
}

#[derive(Debug, Clone, Deserialize)]
pub struct PushSubscription {
    endpoint: String,
    keys: PushSubscriptionKeys,
}

#[derive(Debug, Default)]
struct AppState {
    subscriptions: Vec<PushSubscription>,
    last_ping: Option<Instant>,
}

#[derive(Serialize)]
struct NotificationPayload {
    title: String,
    body: String,
    icon: String,
}

// --- Handlers ---

#[handler]
async fn door_closed(state: Data<&Arc<Mutex<AppState>>>) -> &'static str {
    let mut app_state = state.lock().await;
    app_state.last_ping = Some(Instant::now());
    println!("Ping recebido! Horário salvo.");
    "OK"
}

#[handler]
async fn subscribe(
    state: Data<&Arc<Mutex<AppState>>>,
    payload: Json<PushSubscription>,
) -> Result<&'static str> {
    let mut app_state = state.lock().await;
    let subscription_info = payload.0;
    println!("Novo inscrito: {}", subscription_info.endpoint);
    app_state.subscriptions.push(subscription_info);
    Ok("Subscribed")
}

#[handler]
async fn time(state: Data<&Arc<Mutex<AppState>>>) -> String {
    let app_state = state.lock().await;
    match app_state.last_ping {
        Some(ping_time) => {
            format!(
                "Tempo desde o último ping: {} segundos",
                ping_time.elapsed().as_secs()
            )
        }
        None => "Nenhum ping recebido ainda.".to_string(),
    }
}

#[handler]
async fn reset(state: Data<&Arc<Mutex<AppState>>>) -> Result<&'static str> {
    let mut app_state = state.lock().await;
    let count = app_state.subscriptions.len();
    app_state.subscriptions.clear();
    println!("{} inscritos foram removidos.", count);
    Ok("Reset OK")
}

// <--- MUDANÇA SIGNIFICATIVA AQUI --- >
// O handler `notify_all` recebe a chave como bytes (`Vec<u8>`)
// e usa `from_raw_p256_key` para criar a assinatura.
#[handler]
async fn notify_all(
    state: Data<&Arc<Mutex<AppState>>>,
    vapid_private_key_b64: Data<&String>,
) -> Result<&'static str> {
    let app_state = state.lock().await;

    let payload = NotificationPayload {
        title: "Alerta de Porta!".to_string(),
        body: "A porta foi deixada aberta por muito tempo!".to_string(),
        icon: "https://i.imgur.com/f2j18p8.png".to_string(),
    };
    let payload_json = serde_json::to_string(&payload)
        .map_err(|e| poem::Error::from_string(e.to_string(), StatusCode::INTERNAL_SERVER_ERROR))?;

    for sub in app_state.subscriptions.iter() {
        println!("Enviando notificação para: {}", sub.endpoint);
        let subscription_info = SubscriptionInfo::new(
            sub.endpoint.clone(),
            sub.keys.p256dh.clone(),
            sub.keys.auth.clone(),
        );
        // vapid_private_key_bytes.as_slice(),
        // &subscription_info,

        // <-- MUDANÇA: Usamos `from_raw_p256_key` com os bytes da chave decodificada.
        let signature_builder =
            VapidSignatureBuilder::from_base64(vapid_private_key_b64.as_str(), &subscription_info)
                .map_err(|e| {
                    poem::Error::from_string(e.to_string(), StatusCode::INTERNAL_SERVER_ERROR)
                })?;

        let mut message_builder = WebPushMessageBuilder::new(&subscription_info);

        message_builder.set_payload(ContentEncoding::Aes128Gcm, payload_json.as_bytes());
        message_builder.set_vapid_signature(signature_builder.build().map_err(|e| {
            poem::Error::from_string(e.to_string(), StatusCode::INTERNAL_SERVER_ERROR)
        })?);

        let push_request = message_builder.build().map_err(|e| {
            poem::Error::from_string(e.to_string(), StatusCode::INTERNAL_SERVER_ERROR)
        })?;

        tokio::spawn(async move {
            let client = IsahcWebPushClient::new().unwrap();
            if let Err(e) = client.send(push_request).await {
                eprintln!("Erro ao enviar notificação: {:?}", e);
            }
        });
    }

    Ok("Notificações enviadas")
}

// --- Função `main` ---

// #[tokio::main]
#[shuttle_runtime::main]
async fn poem(
    #[shuttle_runtime::Secrets] secret_store: shuttle_runtime::SecretStore,
) -> ShuttlePoem<impl Endpoint> {
    // Caminho hardcoded para a pasta estática, conforme recomendado pela Shuttle.
    let static_folder = PathBuf::from("static");
    // dotenv().ok(); // removiod

    println!("Tentando resolver o caminho para a pasta estática...");
    match std::fs::canonicalize(&static_folder) {
        Ok(full_path) => {
            println!(
                "[SUCESSO] Caminho absoluto da pasta estática resolvido para: {:?}",
                full_path
            );
        }
        Err(e) => {
            println!(
                "[ERRO] Falha ao resolver o caminho da pasta estática: {}",
                e
            );
        }
    }

    // Pega a chave do SecretStore injetado pelo Shuttle.
    let vapid_private_key_b64 = secret_store
        .get("VAPID_PRIVATE_KEY")
        .expect("VAPID_PRIVATE_KEY deve ser configurada no Secrets.toml");

    let app_state = Arc::new(Mutex::new(AppState::default()));

    let api = Route::new()
        .at("/door-closed", door_closed)
        .at("/subscribe", poem::post(subscribe))
        .at("/time", time)
        .at("/reset", poem::post(reset))
        .at("/notify", poem::post(notify_all));

    // O Shuttle gerencia onde seus arquivos estáticos estão.
    // Usamos o `PathBuf` injetado.
    let static_files = StaticFilesEndpoint::new(static_folder).index_file("index.html");

    let app = Route::new()
        .nest("/api", api)
        .nest("/", static_files)
        .data(app_state)
        .data(vapid_private_key_b64); // Passa a chave para os handlers

    // Em vez de rodar o servidor, eu retorno a instância do app.
    // O Shuttle cuidará de fazer o bind e executar.
    // println!("Servidor rodando em http://127.0.0.1:3000"); // <-- Removido
    // Server::new(TcpListener::bind("127.0.0.1:3000"))
    //     .run(app)
    //     .await

    Ok(app.into())
}
