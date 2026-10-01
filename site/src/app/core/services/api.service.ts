import { Injectable, inject } from '@angular/core';
import { HttpClient, HttpParams, HttpHeaders } from '@angular/common/http';
import { Observable } from 'rxjs';

export interface ReleaseAsset {
  name: string;
  browser_download_url: string;
  size: number;
  download_count: number;
}

export interface GitHubRelease {
  tag_name: string;
  name: string;
  prerelease: boolean;
  published_at: string;
  html_url: string;
  body: string;
  assets: ReleaseAsset[];
}

export interface WorkshopItem {
  id: string;
  user_id?: string | null;
  type: 'set' | 'save' | 'stamp' | 'theme';
  title: string;
  description: string;
  author: string;
  parent_set_id: string | null;
  parent_set_title?: string;
  forked_from_id?: string | null;
  forked_from_title?: string;
  forked_from_author?: string;
  forked_from_version?: number | null;
  forked_from_type?: 'set' | 'save' | 'stamp' | 'theme';
  forks_count?: number;
  version: number;
  set_hash?: string;
  file_path: string;
  file_size: number;
  thumbnail_path?: string;
  meta_json: string;
  likes_count: number;
  favorites_count: number;
  downloads_count: number;
  reports_count: number;
  created_at: string;
  updated_at: string;
  is_liked?: boolean;
  is_favorited?: boolean;
  child_saves_count?: number;
  child_stamps_count?: number;
  is_hidden?: number;
  is_private?: number;
}

export interface ItemsResponse {
  items: WorkshopItem[];
  pagination: {
    page: number;
    limit: number;
    total: number;
    pages: number;
  };
}

export interface User {
  id: string;
  username: string;
  role?: 'admin' | 'moderator' | 'user';
  is_admin: number | boolean;
  created_at: string;
  items_count: number;
  active_sessions_count: number;
}

function getApiBaseUrl(): string {
  if (typeof window !== 'undefined') {
    if (window.location.port === '4200') {
      return (window as any).__SAND3_API_BASE_URL__ || 'http://localhost:3000/api';
    }
    return (window as any).__SAND3_API_BASE_URL__ || '/api';
  }
  return '/api';
}

@Injectable({
  providedIn: 'root'
})
export class ApiService {
  private http = inject(HttpClient);
  private baseUrl = getApiBaseUrl();

  getClientUuid(): string {
    let uuid = localStorage.getItem('sand3_client_uuid');
    if (!uuid) {
      uuid = 'client_' + Math.random().toString(36).substring(2, 15) + Date.now().toString(36);
      localStorage.setItem('sand3_client_uuid', uuid);
    }
    return uuid;
  }

  private getAuthHeaders(): HttpHeaders {
    let headers = new HttpHeaders();
    const token = localStorage.getItem('sand3_token');
    if (token) {
      headers = headers.set('Authorization', `Bearer ${token}`);
    }
    return headers;
  }

  getLatestRelease(): Observable<GitHubRelease> {
    return this.http.get<GitHubRelease>(`${this.baseUrl}/github/latest-release`);
  }

  getAllReleases(): Observable<GitHubRelease[]> {
    return this.http.get<GitHubRelease[]>(`${this.baseUrl}/github/releases`);
  }

  getReadme(): Observable<{ content: string; source: string }> {
    return this.http.get<{ content: string; source: string }>(`${this.baseUrl}/github/readme`);
  }

  getInstructions(): Observable<any> {
    return this.http.get<any>(`${this.baseUrl}/github/instructions`);
  }

  getItems(
    type: string = 'all',
    sort: string = 'popular',
    q: string = '',
    parent_set_id?: string,
    page: number = 1,
    author?: string,
    favorites?: boolean
  ): Observable<ItemsResponse> {
    let params = new HttpParams()
      .set('type', type)
      .set('sort', sort)
      .set('page', page.toString())
      .set('client_uuid', this.getClientUuid());

    if (q) params = params.set('q', q);
    if (parent_set_id) params = params.set('parent_set_id', parent_set_id);
    if (author) params = params.set('author', author);
    if (favorites) params = params.set('favorites', 'true');

    return this.http.get<ItemsResponse>(`${this.baseUrl}/workshop/items`, {
      params,
      headers: this.getAuthHeaders()
    });
  }

  getItem(id: string): Observable<WorkshopItem> {
    const params = new HttpParams().set('client_uuid', this.getClientUuid());
    return this.http.get<WorkshopItem>(`${this.baseUrl}/workshop/items/${id}`, {
      params,
      headers: this.getAuthHeaders()
    });
  }

  getSetSaves(setId: string): Observable<WorkshopItem[]> {
    return this.http.get<WorkshopItem[]>(`${this.baseUrl}/workshop/items/sets/${setId}/saves`, {
      headers: this.getAuthHeaders()
    });
  }

  getSetStamps(setId: string): Observable<WorkshopItem[]> {
    return this.http.get<WorkshopItem[]>(`${this.baseUrl}/workshop/items/sets/${setId}/stamps`, {
      headers: this.getAuthHeaders()
    });
  }

  toggleLike(id: string): Observable<{ is_liked: boolean; likes_count: number }> {
    return this.http.post<{ is_liked: boolean; likes_count: number }>(
      `${this.baseUrl}/workshop/items/${id}/like`,
      { client_uuid: this.getClientUuid() },
      { headers: this.getAuthHeaders() }
    );
  }

  toggleFavorite(id: string): Observable<{ is_favorited: boolean; favorites_count: number }> {
    return this.http.post<{ is_favorited: boolean; favorites_count: number }>(
      `${this.baseUrl}/workshop/items/${id}/favorite`,
      { client_uuid: this.getClientUuid() },
      { headers: this.getAuthHeaders() }
    );
  }

  submitReport(id: string, reason: string, details: string): Observable<any> {
    return this.http.post<any>(
      `${this.baseUrl}/workshop/items/${id}/report`,
      {
        client_uuid: this.getClientUuid(),
        reason,
        details
      },
      { headers: this.getAuthHeaders() }
    );
  }

  getUserFavorites(): Observable<WorkshopItem[]> {
    const params = new HttpParams().set('client_uuid', this.getClientUuid());
    return this.http.get<WorkshopItem[]>(`${this.baseUrl}/workshop/items/user/favorites`, {
      params,
      headers: this.getAuthHeaders()
    });
  }

  publishItem(payload: any): Observable<WorkshopItem> {
    return this.http.post<WorkshopItem>(`${this.baseUrl}/workshop/items`, payload, {
      headers: this.getAuthHeaders()
    });
  }

  updateItem(id: string, payload: any): Observable<WorkshopItem> {
    return this.http.put<WorkshopItem>(`${this.baseUrl}/workshop/items/${id}`, payload, {
      headers: this.getAuthHeaders()
    });
  }

  deleteItem(id: string): Observable<any> {
    return this.http.delete<any>(`${this.baseUrl}/workshop/items/${id}`, {
      headers: this.getAuthHeaders()
    });
  }

  togglePrivateItem(id: string): Observable<{ success: boolean; id: string; is_private: number; message: string }> {
    return this.http.post<{ success: boolean; id: string; is_private: number; message: string }>(
      `${this.baseUrl}/workshop/items/${id}/toggle-private`,
      {},
      { headers: this.getAuthHeaders() }
    );
  }

  getDownloadUrl(id: string): string {
    return `${this.baseUrl}/workshop/items/${id}/download`;
  }

  getThumbnailUrl(id: string): string {
    return `${this.baseUrl}/workshop/items/${id}/thumbnail`;
  }

  private getAdminHeaders(): HttpHeaders {
    let headers = this.getAuthHeaders();
    const adminKey = localStorage.getItem('sand3_admin_key');
    if (adminKey) {
      headers = headers.set('x-admin-key', adminKey);
    }
    return headers;
  }

  checkAdmin(): Observable<{ is_admin: boolean; is_moderator?: boolean; role?: string; user: any }> {
    return this.http.get<{ is_admin: boolean; is_moderator?: boolean; role?: string; user: any }>(`${this.baseUrl}/workshop/admin/check`, {
      headers: this.getAdminHeaders()
    });
  }

  getAdminStats(): Observable<{
    total_items: number;
    total_reports: number;
    total_users: number;
    hidden_items: number;
    reported_items: number;
    total_downloads: number;
  }> {
    return this.http.get<any>(`${this.baseUrl}/workshop/admin/stats`, {
      headers: this.getAdminHeaders()
    });
  }

  getAdminReports(): Observable<{ reports: any[] }> {
    return this.http.get<{ reports: any[] }>(`${this.baseUrl}/workshop/admin/reports`, {
      headers: this.getAdminHeaders()
    });
  }

  dismissReport(reportId: number): Observable<any> {
    return this.http.post<any>(`${this.baseUrl}/workshop/admin/reports/${reportId}/dismiss`, {}, {
      headers: this.getAdminHeaders()
    });
  }

  clearItemReports(itemId: string): Observable<any> {
    return this.http.post<any>(`${this.baseUrl}/workshop/admin/items/${itemId}/clear-reports`, {}, {
      headers: this.getAdminHeaders()
    });
  }

  toggleHideItem(itemId: string): Observable<{ success: boolean; is_hidden: number; message: string }> {
    return this.http.post<any>(`${this.baseUrl}/workshop/admin/items/${itemId}/toggle-hide`, {}, {
      headers: this.getAdminHeaders()
    });
  }

  adminTogglePrivateItem(itemId: string): Observable<{ success: boolean; is_private: number; message: string }> {
    return this.http.post<any>(`${this.baseUrl}/workshop/admin/items/${itemId}/toggle-private`, {}, {
      headers: this.getAdminHeaders()
    });
  }

  adminDeleteItem(itemId: string): Observable<{ success: boolean; message: string }> {
    return this.http.delete<any>(`${this.baseUrl}/workshop/admin/items/${itemId}`, {
      headers: this.getAdminHeaders()
    });
  }

  getAdminItems(q: string = '', type: string = 'all', filter: string = 'all'): Observable<{ items: WorkshopItem[] }> {
    let params = new HttpParams().set('q', q).set('type', type).set('filter', filter);
    return this.http.get<{ items: WorkshopItem[] }>(`${this.baseUrl}/workshop/admin/items`, {
      headers: this.getAdminHeaders(),
      params
    });
  }

  getAdminUsers(q: string = ''): Observable<{ users: User[] }> {
    let params = new HttpParams();
    if (q) params = params.set('q', q);
    return this.http.get<{ users: User[] }>(`${this.baseUrl}/workshop/users`, {
      headers: this.getAdminHeaders(),
      params
    });
  }

  setUserRole(userId: string, role: 'admin' | 'moderator' | 'user'): Observable<{ success: boolean; user_id: string; username: string; role: string; is_admin: boolean; message: string }> {
    return this.http.post<any>(`${this.baseUrl}/workshop/users/${userId}/role`, { role }, {
      headers: this.getAdminHeaders()
    });
  }

  toggleUserAdmin(userId: string): Observable<{ success: boolean; user_id: string; username: string; is_admin: boolean; message: string }> {
    return this.http.post<any>(`${this.baseUrl}/workshop/users/${userId}/toggle-admin`, {}, {
      headers: this.getAdminHeaders()
    });
  }

  userDelete(userId: string): Observable<any> {
    return this.http.delete<any>(`${this.baseUrl}/workshop/users/${userId}`, {
      headers: this.getAdminHeaders()
    });
  }
}