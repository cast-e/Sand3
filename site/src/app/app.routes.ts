import { Routes } from '@angular/router';
import { HomeComponent } from './pages/home/home';
import { WorkshopComponent } from './pages/workshop/workshop';
import { SetDetailComponent } from './pages/set-detail/set-detail';
import { ItemDetailComponent } from './pages/item-detail/item-detail';
import { UploadComponent } from './pages/upload/upload';
import { FavoritesComponent } from './pages/favorites/favorites';
import { AdminComponent } from './pages/admin/admin';

export const routes: Routes = [
  { path: '', component: HomeComponent, title: 'Sand3 — Fast Cellular Automaton' },
  { path: 'workshop', component: WorkshopComponent, title: 'Sand3 — Community Workshop' },
  { path: 'workshop/sets/:id', component: SetDetailComponent, title: 'Sand3 — Workshop Set Details' },
  { path: 'workshop/items/:id', component: ItemDetailComponent, title: 'Sand3 — Item Details' },
  { path: 'workshop/upload', redirectTo: 'workshop' },
  { path: 'workshop/favorites', component: FavoritesComponent, title: 'Sand3 — Favorites' },
  { path: 'workshop/admin', component: AdminComponent, title: 'Sand3 — Admin Moderation' },
  { path: 'admin', component: AdminComponent, title: 'Sand3 — Admin Dashboard' },
  { path: '**', redirectTo: '' }
];